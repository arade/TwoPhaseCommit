#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/signal.h>
#include "./server.h"

#ifndef SIG_PF
#define SIG_PF void(*)(int)
#endif
#define IP "127.0.0.1"
#define SIZE 1000	//size of the db

//here we initialize the global variables we use in a given server process
int static SERVERPROG;
int static db_state;//current state of the db
int static global_ID;//the last global id given
int static local_ID;//the last local id given
char static db_name[40];//name of the db
char static db_state_name[40];//name of state log file
char static global_log_name[40];//name of global log file 
char static local_log_name[40];//name of local log file
char static tempdbb_name[40];//a name holder for the temporary db's we create
char static DMlistt_name[40];//a name holder for the temporary DM's list in coordinator
char static recoveryy_name[40];//a name holder for the temporary DM's list in the voting DM for recovery in case of failur
char static db_subdirectory[20];// a name holder for our sub directory to store the DB files
char static full_path[60];// a name holder for files
char static blockinglistt_name[40];// a name holder for blocked keys in a process
char static blockinglocal_name[40];// a name holder for blocked keys in a process
char static blockingglobal_name[40];// a name holder for blocked keys in a process
int static blockinglist[SIZE];
int static blockingglobal[SIZE];
int static blockinglocal[SIZE];
int static blockinglist_len;
int static blockingglobal_len;
int static blockinglocal_len;
SVCXPRT *transp;

//structure of the valu to be stored in the database
struct value_state{
	int value;
	int state;//the state of the key which it was last modified
};

//-----------------here we define local helper ------------------------------
void DBerror(int type){
switch (type){
	case 0 :
		printf("error message 0\n");
	break;
	case 1 :
		printf("error in reading from database\n");
	break;
	case 2 :
		printf("Can't initialize DB!\n");
	break;
	case 3 :
		printf("error in writing temporary modification of database\n");
	break;
	case 4 :{
		printf("Can't register DM on coordinator\n");
		//exit(0);
		}
	break;
	case 5 :{
		printf("Can't connect to the server process\n");
		//exit(0);
		}
	case 6 :{
		printf("Can't open file\n");
		//exit(0);
		}
	break;		
	default :
		printf("Error encaunterd while processing your request\n");
	}
}//end of DBerror

/*	The following 4 functions dynamically generate file names for 
	different perpouses in our code                    */
//-------------- add sub directory path to the name of files-------------
char *add_path(char* Filename){
	memset(full_path, 0, 60);	
	sprintf(full_path,"db_%d/", SERVERPROG);
	return strcat(full_path, Filename);
}//end of add_path

//----------------give name to temporary db file ------------------------
char *tempdb_name(int local_id){
	memset(tempdbb_name, 0, 40);
	sprintf(tempdbb_name, "tempdb_%d_%d.tmp", SERVERPROG,local_id);
	return tempdbb_name;
}//end of tempdb_name

//-------------give name to temporary DM register file ------------------
char *DMlist_name(int id){
	memset(DMlistt_name, 0, 40);
	sprintf(DMlistt_name, "DMlistt_%d_%d.tmp", SERVERPROG,id);
	return DMlistt_name;
}//end of tempdb_name

//----------------give name to temporary recovery file ------------------
char *recovery_name(int local_id){
	memset(recoveryy_name, 0, 40);
	sprintf(recoveryy_name, "recoverylist_%d_%d.tmp", SERVERPROG,local_id);
	return recoveryy_name;
}//end of recovery_namempdb_name

//here we define a structure to be used in db write/read functions
FILE *FileOpen(char* Filename, int options)
{
FILE* pFile;
char *path;
path=add_path(Filename);
switch(options){
	case 0 :{
		pFile = fopen(path,"rb+");
		if (!pFile)
			pFile = fopen(path,"wb+");}
	break;
	case 1 :{
		pFile = fopen(path,"a+");
		if (!pFile)
			pFile = fopen(path,"w+");}
	break;
	case 2 :
		pFile = fopen(path,"w");
	break;
	case 3 :
		pFile = fopen(path,"r");
	break;
	case 4 :{
		pFile = fopen(path,"r+");
		if (!pFile)
			pFile = fopen(path,"w+");}
		}
return pFile; 
}// end of FileOpen

//-----------here we calculate the length of a file-------
int file_len(FILE* file){
	int len;
	fseek(file, 0, SEEK_END);
	len = ftell(file);
	fseek(file, 0, SEEK_SET);
	return len;
}//end of file_len checked

//----- This function  check if a file is empty ---------------
bool is_empty(FILE *file)
{
    if (file_len(file) == 0)
		return true;
    return false;
}//end of is_empty

//--------------- Write a record to the DB---------------------
bool write_db(FILE *File, int key, struct value_state data){
	if( fseek(File, key * sizeof(data), SEEK_SET) == 0 )
		if ( fwrite(&data,sizeof(data),1,File) )
			return true;
	return false; 
}// end of write_db
 
//-------------- read a record from the db---------------------
struct value_state read_db(FILE *File, int key){
	struct value_state data;
	if( fseek(File, key * sizeof(data), SEEK_SET) == 0 )
		fread(&data,sizeof(data),1,File) ;
	else 
		DBerror(1);
	return data;
}// end of read_db

//--------------------initialize DB----------------------------
void init_db(FILE* File){  
	if(is_empty(File)){
		printf("Initializing DB...\n");
		struct value_state data;
		int x;
		data.value=0;
		data.state=db_state;
		memset(&data,sizeof(data),0);
		for (x = 1; x < SIZE; x++){
		   if (! write_db(File,x,data))
			DBerror(2);		
		} //end of for loop
		printf("Initializing DB finished!\n");
	}//end of if
 }//end of init_db
 
//--------------initialize state of DB--------------------------
void init_db_state(FILE* File)
 {
	if(is_empty(File)){
		printf("Initializing DB state...\n");
   		fprintf(File, "%i\t%i\t%i\t",0,0,0);//state,local_id,global_id
   		printf("Initializing DB state finished!\n");}    		 
	fscanf(File, "%i\t%i\t%i\t",&db_state,&global_ID,&local_ID);
	if(is_empty(File))
		DBerror(2);
	printf("DB state:%i global ID %i Local Id %i\n",db_state,global_ID,local_ID); 
} //end of init_db_state

// -------------System initializer-----------------------------
void dm_initialize(void){
	FILE *temp;
	sprintf(db_subdirectory,"db_%d", SERVERPROG);
	sprintf(db_name, "db_%d.dat",  SERVERPROG);
	sprintf(db_state_name, "db_state_%d.dat",  SERVERPROG); 
	sprintf(global_log_name, "global_%d.log",  SERVERPROG);
	sprintf(local_log_name, "local_%d.log",  SERVERPROG);
	sprintf(blockinglistt_name, "blockinglist_%d.log", SERVERPROG);
	sprintf(blockingglobal_name, "blockingglobal_%d.log", SERVERPROG);
	sprintf(blockinglocal_name, "blockinglocal_%d.log", SERVERPROG);
	
	mkdir(db_subdirectory,0777);// we creat a sub directory for the files if it dont exist
	
	temp=FileOpen(db_name,0);
	init_db(temp);
	fclose(temp);
	
	temp=FileOpen(db_state_name,4);
	init_db_state(temp);
	fclose(temp);
	
	fclose(FileOpen(global_log_name,1));	
	fclose(FileOpen(local_log_name,1));
	fclose(FileOpen(blockinglistt_name,4));
	fclose(FileOpen(blockingglobal_name,4));
	fclose(FileOpen(blockinglocal_name,4));
}//end of dm_initialize

//----------update the state of db-----------------------------
void update_db_state(void){
	FILE *File;
	File=FileOpen(db_state_name,0);		
	if(fprintf(File, "%i\t%i\t%i\t",db_state,global_ID,local_ID)<0)
		DBerror(3);
	fclose(File);
}//end of update_db_state

//----------- read db state------------------ 
int read_db_state(int options){
	int state,global_id,local_id;
	FILE* File;
	File=FileOpen(db_state_name,1);
	fscanf(File, "%d\t%d\t%d", &state, &local_id,&global_id );
	//DBerror(4);
	switch(options){
		case 0 :
			return state;
		break;
		case 1 :
			return local_id;
		break;
		case 2 :
			return global_id;
		break;
		default :
			return 0;
	} // end of switch
 } //end of read_db_state
 	
//-------------- global Log function--------------------------
void *log_global(int global_id,int type)
{
	FILE *logfile;
	logfile=FileOpen(global_log_name,1);
	if(fprintf(logfile, "%i\t%i\n",global_id,type)<0)
		DBerror(5);
	fclose(logfile);
}//end of log_global

//-------------- local Log function-------------------------
void *log_local(int local_id,int type)
{
	FILE *logfile;
	logfile=FileOpen(local_log_name,1);
	if(fprintf(logfile, "%i\t%i\n",local_id,type)<0)
		DBerror(5);
	fclose(logfile);
}//end of log_local

//----------show elements of a file----------------------
void show_file(FILE *file){
	int len,temp;
	len=file_len(file);
	printf("value of the file are\n");
	while(ftell(file)<len){
		fscanf(file, "%d\n", &temp);
		printf("%d\t",temp); 
		}
	printf("\n");
	fseek(file, 0, SEEK_SET);
}//end of show_file checked

//-----------show elements of the array--------- blockinglist_len
void show_array(int *val, int len){
	int i;
	printf("value of the array are\n");
	for (i=0;i<len;i++)
		printf("%d\t",val[i]);
	printf("\n");
}//end of show_array checked

//---------------load from file---------------------------
int load_from_file(char *filename, int* arrayname) {
	FILE *file;
	file=FileOpen(filename,3);
	//printf("loading from file!\n");
	int len,index=0;
	memset(arrayname, 0, SIZE * sizeof(int));
	len = file_len(file);
	while(ftell(file)<len){
		fscanf(file, "%d\n", &arrayname[index]);
		index++;
		}
	fclose(file);
	return index;
}//end of load_from_file

//---------------load to file---------------------------
void load_to_file(char *filename, int* arrayname, int len) {
	FILE *file;
	file=FileOpen(filename,2);
	//printf("loading to file!\n");
	int index=0;
	while(index<len){
		fprintf(file,"%d\n",arrayname[index]);
		index++;
		}
	//show_file(file);
	fclose(file);
}// end of load_to_file

bool check_blocking(int key, int *array, int lenght){
	int index;	
	for (index=0; index<lenght; index++) {
		if (array[index] == key)
			return true;
		}
	return false;
}//end of check_blocking checked

//find index of of an element in an array
int find_index(int key, int *array, int lenght){
	int index=0;
	for (; index<=lenght; index++) {
		if (array[index] == key)
			return index;
		}
	return index;
}//end of find_index checked

//-------add an element to an of array--------------------
int add_to_list(int key,int *listname, int lenght){
	listname[lenght]=key;
	lenght++;
	return lenght;
}//end of add_to_list

//--------remove an element to an of array----------------
int remove_from_list(int key,int *listname, int lenght){
	int i;
	i=find_index(key,listname,lenght);
	listname[i]=listname[lenght];
	listname[lenght]=0;
	lenght--;
	return lenght;
}//end of remove_from_list

//----------here we add new  blocking key list -------------
void addto_blockinglist(int *newlist, int length){
	int i,j;
	//printf("we are in add to blocking list!\n");
	for(i=0; i<blockinglist_len;i++)
	 	;
	for (j=0;j<length && i<SIZE;i++,j++){
		blockinglist[i]=newlist[j];
		blockinglist_len++;
		}
	//show_array(blockinglist,blockinglist_len);//test
	load_to_file(blockinglistt_name,blockinglist, blockinglist_len);
}//end of addto_blockinglist checked

//here we remove  blocking keys from the list --------------
void removefrom_blockinglist(int *removelist, int length){
	//printf("we are in remove blocking!\n");
	int index,i,j,k=0;
	for (j=0;j<length;j++){
		index=find_index(removelist[j], blockinglist, blockinglist_len);
		blockinglist[index]=blockinglist[blockinglist_len];
		blockinglist[blockinglist_len]=0;
		blockinglist_len--;
		}
	//show_array(blockinglist,blockinglist_len);
	load_to_file(blockinglistt_name,blockinglist, blockinglist_len);		
}//end of removefrom_blockinglist

//-----check weather to commit or not ----------------------
bool commit_check(int local_id){
	FILE *db,*tempdb;
	int temp_key,temp_value,temp_state,index=0;
	struct value_state db_data;
	int templist[SIZE]={0};
	
	db=FileOpen(db_name,0);	
	tempdb=FileOpen(tempdb_name(local_id),1);
	blockinglist_len=load_from_file(blockinglistt_name,blockinglist);	
	
	while (fscanf(tempdb, "\t%d\t%d\t%d", &temp_key,&temp_value, &temp_state) != EOF){
			
		db_data=read_db(db, temp_key);
		if(db_data.state>temp_state){
			printf("commit can't procced because one of the key's state has changed\n");
			fclose(db);
			fclose(tempdb);
			return false;}		
		if(check_blocking(temp_key,blockinglist,blockinglist_len)){
			printf("commit can't procced because one of the key is in blocking list\n");
			fclose(db);
			fclose(tempdb);
			return false;}		
		templist[index]=temp_key;
		index++;		
		}//end of while	
	addto_blockinglist(templist,index);
	printf("Ready to commit... \n");	
	fclose(tempdb);
	fclose(db);
	return true;
}//end of commit_check

//-------- update the main db(for commit)---------------
int update_db(int local_id){
	FILE *db,*tempdb;
	int temp_key,temp_value,temp_state,index=0;
	struct value_state db_data;
	int templist[SIZE]={0};	
	
	printf("commiting.... \n");
	db=FileOpen(db_name,0);	
	tempdb=FileOpen(tempdb_name(local_id),1);
	db_state=read_db_state(0);
	db_state++;
	update_db_state();
	while (fscanf(tempdb, "\t%d\t%d\t%d", &temp_key,&temp_value, &temp_state) != EOF){
		db_data.value=temp_value;
		db_data.state=db_state;
		write_db(db,temp_key, db_data);
		templist[index]=temp_key;
		index++;}//end of while
	log_local(local_id,1);
	removefrom_blockinglist(templist,index);
	fclose(tempdb);
	fclose(db);
	return 1;
}//end of update_db

//-------- remove the ---------------
void abort_update(int local_id){
	FILE *tempdb;
	int temp_key,temp_value,temp_state,len,index=0;
	int templist[SIZE]={0};
	
	printf("Aborting.... \n");
	tempdb=FileOpen(tempdb_name(local_id),1);

	while (fscanf(tempdb, "\t%d\t%d\t%d", &temp_key,&temp_value, &temp_state) != EOF){
		printf("key= %d value= %d state= %d\n",temp_key,temp_value, temp_state);
		templist[index]=temp_key;
		index++;
		}//end of while
	log_local(local_id,0);
	removefrom_blockinglist(templist,index);
	fclose(tempdb);
}//end of update_db

//-------- show update the main db(for commit)---------------
void update_show(int local_id){
	printf("updated values are:\n");
	FILE *db,*tempdb;
	int temp_key,temp_value,temp_state;
	struct value_state db_data;
	
	db=FileOpen(db_name,0);	
	tempdb=FileOpen(tempdb_name(local_id),1);		
	while (fscanf(tempdb, "\t%d\t%d\t%d", &temp_key,&temp_value, &temp_state) != EOF){
		db_data=read_db(db, temp_key);
		printf("key= %d value= %d state= %d\n", temp_key,db_data.value,db_data.state);
		}//end of while
	fclose(tempdb);
	fclose(db);
}//end of update_show

//------ to finish commiting blocked local ids-----------------------------
bool finish_blocked_lid(int id, int type){
	FILE *recovery;
	int len, local_id,prognum,x;
	bool abort=false;
	recovery=FileOpen(recovery_name(id),1);
	len =  file_len(recovery);
	char* out_1,out_2;

	while(ftell(recovery)<len && (!abort)){
		fscanf(recovery,"\t%d\t%d",&local_id, &prognum);
		if(prognum!=SERVERPROG){
		printf("server to connet to %d localid %d to request decision state\n",prognum,local_id);
			CLIENT *cohort;
			cohort = clnt_create(IP,prognum,SERVERVERS, "udp");					
			if (cohort == NULL){ 
				clnt_pcreateerror(IP);	
				goto out_1;}
			x=*decision_state_1(&local_id,cohort);
			printf("replyed state %d\n",x);
			if(x==0 || x==1 || x==3)
				abort=true;
		out_1:	clnt_destroy (cohort);
			}
		}//end of while*/
	if(abort){
			fseek(recovery, 0, SEEK_SET);
			struct commit_struct decison_l;
			if(x==0 || x==3)
				decison_l.commit=false;
			else if(x==1 )
				decison_l.commit=true;
			
			if(type == 0){
				
				while(ftell(recovery)<len){
					fscanf(recovery,"\t%d\t%d",&local_id, &prognum);
					printf("server to connet to %d local_id %d to commit by the new coordinator\n",prognum,local_id);
					decison_l.id=local_id;
					decison_l.type=1;
					if(prognum!=SERVERPROG){							
						CLIENT *cohort;
						cohort = clnt_create(IP,prognum,SERVERVERS, "tcp");					
						if (cohort == NULL){
							clnt_pcreateerror(IP);	
							}							
						commit_local_1(&decison_l,cohort);								
						clnt_destroy (cohort);
						}//end of if
					else {
						struct svc_req *req;
						decison_l.type=2;
						commit_local_1_svc(&decison_l,req);
						}							
					}//end of while*/
				fclose(recovery);	
				remove(add_path(recovery_name(id)));
				}//end of if(type == 0)
			else{
				decison_l.type=1;
				decison_l.id=id;
				printf("commiting local_id %d in recovery\n",decison_l.id);
				struct svc_req *req;
				commit_local_1_svc(&decison_l,req);
				remove(add_path(recovery_name(id)));
				fclose(recovery);
				}									
			}//end of if(abort)
		//fclose(recovery);
		return abort;
}//end of void finish_blocked_lid

//----------------during time out contact other cohort for state to commit-----------------------
void timeout_proc2(){
	printf("cheaking\n");
	blockinglocal_len = load_from_file(blockinglocal_name, blockinglocal);
	int i,l_id;
	for (i=0;i<blockinglocal_len;){
		l_id=blockinglocal[i];
		if(finish_blocked_lid(l_id, 0)){
			blockinglocal_len=remove_from_list(l_id,blockinglocal, blockinglocal_len);
			load_to_file(blockinglocal_name, blockinglocal, blockinglocal_len);
			}
		else
			i++;
		}
	signal(SIGALRM, timeout_proc2);
        alarm(30);//set timeout for waiting coordinator decision
}//end of timeout_proc()

//------------ a recovery procedure during server startup----------------------
void recover_proc(){
	dm_initialize();		
	blockingglobal_len = load_from_file(blockingglobal_name, blockingglobal);
	while (blockingglobal_len>0){
		int local_id, prognum,len,g_id,len_log, temp_id,state;
		bool abort=true;
		g_id=blockingglobal[0];
		printf("we are in recovery...\n");
		
		FILE *logfile;	
		logfile=FileOpen(global_log_name,4);
		len_log=file_len(logfile);
		while(ftell(logfile)<len_log && abort){
			fscanf(logfile,"%i\t%i\n",&temp_id,&state);
			if(g_id==temp_id)
				abort=false;
			}
		fclose(logfile);
				
		struct commit_struct decison_l;
		decison_l.type=1;
		if(state==3)
			decison_l.commit=true;
		else
			decison_l.commit=false;
			
		FILE *DMlist;	
		DMlist=FileOpen(DMlist_name(g_id),4);	
		len = file_len(DMlist);	
		while(ftell(DMlist)<len){
			fscanf(DMlist,"\t%d\t%d",&local_id, &prognum);			
			printf("server to connet to %d localid %d for a commit. recovery...\n",prognum,local_id);
			decison_l.id=local_id;
			if(prognum!=SERVERPROG){
				CLIENT *local;
				local = clnt_create(IP,prognum,SERVERVERS, "udp");
				if (local == NULL)
					clnt_pcreateerror(IP);			
				commit_local_1(&decison_l,local);
				clnt_destroy (local);
				}//end of if
			else	{
				struct svc_req *req;
				commit_local_1_svc(&decison_l,req);
				}
			}//end of while(ftell(DMlist)<len)
		fclose(DMlist);
		remove(add_path(DMlist_name(g_id)));
		blockingglobal_len=remove_from_list(g_id,blockingglobal, blockingglobal_len);
		load_to_file(blockingglobal_name, blockingglobal, blockingglobal_len);
		}
	blockinglocal_len = load_from_file(blockinglocal_name, blockinglocal);
	while (blockinglocal_len>0){
		int l_id;
		l_id=blockinglocal[0];
		finish_blocked_lid(l_id, 1);
		blockinglocal_len=remove_from_list(l_id,blockinglocal, blockinglocal_len);
		load_to_file(blockinglocal_name, blockinglocal, blockinglocal_len);
	}
printf("Recovery finished!\n");
}//end of recover_proc

//---------------end of local helper functions ----------------------------------------------------------

// ----------------server functions used by client--------------------------------------------------------

//---------------------get local id--------------------------------------
int * get_local_id_1_svc(struct to_localID *request_LID, struct svc_req *req)
{
	if(local_log_name && local_log_name[0] == '\0')//if it is for the first time we initialize the DB
		dm_initialize();
	local_ID++;
	update_db_state();	
	if(request_LID->trans_type==1){
		struct coord_reg to_coord;
		int *check;
		to_coord.global_id=request_LID->global_id;
		to_coord.local_id=local_ID;
		to_coord.prognum=SERVERPROG;		
		printf("Registering Cohort on coordinator...\n");
		if(SERVERPROG!=request_LID->global_prognum){
			CLIENT *global;
			global = clnt_create(IP,request_LID->global_prognum,SERVERVERS, "udp");
			if (global == NULL) 
				clnt_pcreateerror(IP);
			check=register_dm_1(&to_coord, global);
			clnt_destroy (global);
			}
		else {
			struct svc_req *req;
			check=register_dm_1_svc(&to_coord, req);
			}			
		if (*check!=1)
			DBerror(4);
		}
	//update_db_state();
	printf("Given local ID= %d \n", local_ID);
	return &local_ID;
}//end of get_local_id_1_svc

//---------------------get global id-----------------
int * get_global_id_1_svc(void *arg, struct svc_req *req)
{
	if(global_log_name && global_log_name[0] == '\0' )//if it is for the first time we initialize the DB
		dm_initialize();
	global_ID++;
	update_db_state();
	printf("Given global ID= %d \n", global_ID);
	return &global_ID;
}//end of get_global_id_1_svc

int * modify_db_1_svc(to_dm  *db_mod, struct svc_req *req)
{
	static return_val;
	return_val=1;
	FILE *tempdb;// here we open a temporary file to store the changes made by the client untill it commit
	tempdb=FileOpen(tempdb_name(db_mod->local_id),1);
	if(!tempdb){
		DBerror(6);
		return_val=0;
		return &return_val;}	
	db_state=read_db_state(0);
	fprintf(tempdb, "\t%d\t%d\t%d",db_mod->key,db_mod->value,db_state);
	fclose(tempdb);
	return &return_val;
}//end of modify_db_1_svc

int * commit_local_1_svc(commit_struct *decison, struct svc_req *req)
{	
	static return_val;	
	return_val=1;
	if(decison->type==0) {
		bool check=true;
		if(decison->commit){
			if(check=commit_check(decison->id))
				update_db(decison->id);
			}
		else{
			return_val=0;
			log_local(decison->id,0);
			}
		update_show(decison->id);	
		blockinglocal_len=remove_from_list(decison->id,blockinglocal, blockinglocal_len);
		load_to_file(blockinglocal_name, blockinglocal, blockinglocal_len);
		remove(add_path(tempdb_name(decison->id)));
		}
	else {
		blockinglocal_len = load_from_file(blockinglocal_name, blockinglocal);
		if(check_blocking(decison->id, blockinglocal, blockinglocal_len)){
			if(decison->commit)				
				update_db(decison->id);
			else{
				abort_update(decison->id);
				return_val=0;
				}
			update_show(decison->id);	
			blockinglocal_len=remove_from_list(decison->id,blockinglocal, blockinglocal_len);
			load_to_file(blockinglocal_name, blockinglocal, blockinglocal_len);
			remove(add_path(tempdb_name(decison->id)));
			if(decison->type==1)
				remove(add_path(recovery_name(decison->id)));				
			}
		else
			return_val=0;			
		}
return &return_val;
}//end of commit_local_1_svc

//------------here 2PC is implimented-----------------------
int * commit_global_1_svc(commit_struct *decison, struct svc_req *req)
{		
	int local_id, prognum,len;
	bool abort=false;
	FILE *DMlist;
	static int return_val;
	return_val=1;	

	//--------------requst for vote phase------------------------------
	if(decison->commit){
		struct vote_req to_dm;
		int timeout=1;
		char temp_DMlist[500];
		
		DMlist=FileOpen(DMlist_name(decison->id),4);
		fgets (temp_DMlist , 500 , DMlist);
		printf("Requesting for vote...\n");
		len = file_len(DMlist);
		to_dm.DM_list=temp_DMlist;		
		while(ftell(DMlist)<len && !abort){
			fscanf(DMlist,"\t%d\t%d",&local_id, &prognum);			
			//printf("Server to connet to %d localid %d to request vote\n",prognum,local_id);
			to_dm.local_id=local_id;
			timeout++;				
			to_dm.time_out=5+timeout*5;
			if(prognum!=SERVERPROG){
				CLIENT *global;
				global = clnt_create(IP,prognum,SERVERVERS, "udp");
				if (global == NULL){
					clnt_pcreateerror(IP);
					abort=true;}
				if((*give_vote_1(&to_dm,global)==1))
					abort=true;
				clnt_destroy (global);
				}
			else	{
				struct svc_req *req;
				if((*give_vote_1_svc(&to_dm,req)==1))
					abort=true;
				}
			}//end of while
		if(abort)
			printf("Server with program number: %d vote abort!\n",prognum);
		else
			printf("All the servers vote commit!\n");
		fclose(DMlist);		
		}//end if (decison->commit)

	if(abort || (!decison->commit))
		log_global(decison->id,2);
	else 
		log_global(decison->id,3);
	blockingglobal_len=add_to_list(decison->id,blockingglobal, blockingglobal_len);
	load_to_file(blockingglobal_name, blockingglobal, blockingglobal_len);
	//show_array(blockingglobal, blockingglobal_len);

	if(abort || (!decison->commit))
		return_val=0;
	int child_id;
	child_id = fork();
	if ((child_id==0)){
		//pmap_unset (SERVERPROG, SERVERVERS);
		//svc_done(transp);
		svc_exit ();
		return &return_val;
		//exit(0);
		}
	else {
		//-----------final commit phase-------------------	
		struct commit_struct decison_l;
		decison_l.commit=(decison->commit && !abort);
		decison_l.type=1;
		DMlist=FileOpen(DMlist_name(decison->id),4);
		len = file_len(DMlist);
		int crush_at,test=0;
		printf("Please enter when to crush: ");
		scanf("%d",&crush_at);
		char *out;
		while(ftell(DMlist)<len){
			if(crush_at<test){			
				pmap_unset (SERVERPROG, SERVERVERS);
				//xprt_unregister (transp);
				svc_exit();
				exit(0);
				}
			test++;
			fscanf(DMlist,"\t%d\t%d",&local_id, &prognum);			
			printf("Connecting Cohort with program no: %d & localid %d to finalize distributed transaction\n",prognum,local_id);
			decison_l.id=local_id;
			if(prognum!=SERVERPROG){
				CLIENT *local;
				local = clnt_create(IP,prognum,SERVERVERS, "udp");
				if (local == NULL)
					clnt_pcreateerror(IP);			
				commit_local_1(&decison_l,local);
				clnt_destroy (local);
				}//end of if
			else	{
				struct svc_req *req;
				commit_local_1_svc(&decison_l,req);
				}
			}//end of while
		fclose(DMlist);
		remove(add_path(DMlist_name(decison->id)));
		blockingglobal_len=remove_from_list(decison->id,blockingglobal, blockingglobal_len);
		load_to_file(blockingglobal_name, blockingglobal, blockingglobal_len);
		//show_array(blockingglobal, blockingglobal_len)
	}
}//end of commit_global_1_svc

int * register_dm_1_svc(coord_reg *reg_dm, struct svc_req *req)
{
	static return_val;
	return_val=1;
	printf("Registering cohort with program number: %d & local ID: %d\n",reg_dm->prognum,reg_dm->local_id);
	FILE *DMlist;
	DMlist=FileOpen(DMlist_name(reg_dm->global_id),1);
	if(!DMlist){
		DBerror(6);
		return_val=0;
		return &return_val;
		}
	fprintf(DMlist, "\t%i\t%i",reg_dm->local_id,reg_dm->prognum);
	fclose(DMlist);
	return &return_val;
}//end of register_dm_1_svc

int * give_vote_1_svc(vote_req *vote_request, struct svc_req *req)
{
	static int return_val;
	FILE *recovery;
	return_val=0;
	recovery=FileOpen(recovery_name(vote_request->local_id),1);
	fprintf(recovery, "%s",vote_request->DM_list);
	fclose(recovery);	
		if (!commit_check(vote_request->local_id))
		return_val=1;
	else{
		blockinglocal_len = load_from_file(blockinglocal_name, blockinglocal);
		blockinglocal_len = add_to_list(vote_request->local_id,blockinglocal, blockinglocal_len);
		load_to_file(blockinglocal_name, blockinglocal,blockinglocal_len);	
		}
	if(return_val==0)
		printf("giving vote: commit\n");
	else
		printf("giving vote: abort\n");
	/*signal(SIGALRM, timeout_proc);
        alarm(vote_request->time_out);//set timeout for waiting coordinator decision*/
        return &return_val;

}//end of register_dm_1_svc

int * decision_state_1_svc(int *local_id, struct svc_req *req)
{
	static int return_val;	
	return_val=3;
	printf("Giving state of local ID: %d\n",*local_id);
	if(check_blocking(*local_id,blockinglocal,blockinglocal_len)){
		return_val=2;
		return &return_val;
		}					
	int len, temp_id,decision;
	FILE *logfile;
	logfile=FileOpen(local_log_name,1);
	len=file_len(logfile);
	while(ftell(logfile)<len){
		fscanf(logfile,"%i\t%i\n",&temp_id,&decision);
		if(*local_id==temp_id){
			return_val=decision;
			return &return_val;
			}
		}
	return &return_val;	
}//end of register_dm_1_svc

//***************************************************************************************
//********************here are the codes generated by rpcgen*****************************
//***************************************************************************************

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

int *
get_local_id_1(to_localID *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call (clnt, GET_LOCAL_ID,
		(xdrproc_t) xdr_to_localID, (caddr_t) argp,
		(xdrproc_t) xdr_int, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
get_global_id_1(void *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call (clnt, GET_GLOBAL_ID,
		(xdrproc_t) xdr_void, (caddr_t) argp,
		(xdrproc_t) xdr_int, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
modify_db_1(to_dm *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call (clnt, MODIFY_DB,
		(xdrproc_t) xdr_to_dm, (caddr_t) argp,
		(xdrproc_t) xdr_int, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
commit_local_1(commit_struct *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call (clnt, COMMIT_LOCAL,
		(xdrproc_t) xdr_commit_struct, (caddr_t) argp,
		(xdrproc_t) xdr_int, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
commit_global_1(commit_struct *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call (clnt, COMMIT_GLOBAL,
		(xdrproc_t) xdr_commit_struct, (caddr_t) argp,
		(xdrproc_t) xdr_int, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
register_dm_1(coord_reg *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call (clnt, REGISTER_DM,
		(xdrproc_t) xdr_coord_reg, (caddr_t) argp,
		(xdrproc_t) xdr_int, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
give_vote_1(vote_req *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call (clnt, GIVE_VOTE,
		(xdrproc_t) xdr_vote_req, (caddr_t) argp,
		(xdrproc_t) xdr_int, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

int *
decision_state_1(int *argp, CLIENT *clnt)
{
	static int clnt_res;

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call (clnt, DECISION_STATE,
		(xdrproc_t) xdr_int, (caddr_t) argp,
		(xdrproc_t) xdr_int, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}
//**********************************************************************

bool_t
xdr_to_localID (XDR *xdrs, to_localID *objp)
{
	register int32_t *buf;

	 if (!xdr_int (xdrs, &objp->trans_type))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->global_id))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->global_prognum))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_to_dm (XDR *xdrs, to_dm *objp)
{
	register int32_t *buf;

	 if (!xdr_int (xdrs, &objp->key))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->value))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->local_id))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_commit_struct (XDR *xdrs, commit_struct *objp)
{
	register int32_t *buf;

	 if (!xdr_int (xdrs, &objp->id))
		 return FALSE;
	 if (!xdr_bool (xdrs, &objp->commit))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->type))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_coord_reg (XDR *xdrs, coord_reg *objp)
{
	register int32_t *buf;

	 if (!xdr_int (xdrs, &objp->global_id))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->local_id))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->prognum))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_vote_req (XDR *xdrs, vote_req *objp)
{
	register int32_t *buf;

	 if (!xdr_int (xdrs, &objp->local_id))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->time_out))
		 return FALSE;
	 if (!xdr_string (xdrs, &objp->DM_list, ~0))
		 return FALSE;
	return TRUE;
}

static void
serverprog_1(struct svc_req *rqstp, register SVCXPRT *transp)
{
	union {
		to_localID get_local_id_1_arg;
		to_dm modify_db_1_arg;
		commit_struct commit_local_1_arg;
		commit_struct commit_global_1_arg;
		coord_reg register_dm_1_arg;
		vote_req give_vote_1_arg;
		int decision_state_1_arg;
	} argument;
	char *result;
	xdrproc_t _xdr_argument, _xdr_result;
	char *(*local)(char *, struct svc_req *);

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply (transp, (xdrproc_t) xdr_void, (char *)NULL);
		return;

	case GET_LOCAL_ID:
		_xdr_argument = (xdrproc_t) xdr_to_localID;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) get_local_id_1_svc;
		break;

	case GET_GLOBAL_ID:
		_xdr_argument = (xdrproc_t) xdr_void;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) get_global_id_1_svc;
		break;

	case MODIFY_DB:
		_xdr_argument = (xdrproc_t) xdr_to_dm;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) modify_db_1_svc;
		break;

	case COMMIT_LOCAL:
		_xdr_argument = (xdrproc_t) xdr_commit_struct;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) commit_local_1_svc;
		break;

	case COMMIT_GLOBAL:
		_xdr_argument = (xdrproc_t) xdr_commit_struct;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) commit_global_1_svc;
		break;

	case REGISTER_DM:
		_xdr_argument = (xdrproc_t) xdr_coord_reg;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) register_dm_1_svc;
		break;

	case GIVE_VOTE:
		_xdr_argument = (xdrproc_t) xdr_vote_req;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) give_vote_1_svc;
		break;

	case DECISION_STATE:
		_xdr_argument = (xdrproc_t) xdr_int;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) decision_state_1_svc;
		break;

	default:
		svcerr_noproc (transp);
		return;
	}
	memset ((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
		svcerr_decode (transp);
		return;
	}
	result = (*local)((char *)&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, (xdrproc_t) _xdr_result, result)) {
		svcerr_systemerr (transp);
	}
	if (!svc_freeargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
		fprintf (stderr, "%s", "unable to free arguments");
		exit (1);
	}
	return;
}


//****************************************************************************************
//*****************here the main function begins******************************************

int
main (int argc, char **argv)
{
	
	if(argv[1])	
		SERVERPROG = atoi(argv[1]);
	else
		SERVERPROG = 200;

	pmap_unset (SERVERPROG, SERVERVERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		fprintf (stderr, "%s", "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, SERVERPROG, SERVERVERS, serverprog_1, IPPROTO_UDP)) {
		pmap_unset(SERVERPROG, SERVERVERS);
		transp = svcudp_create(RPC_ANYSOCK);
		if (!svc_register(transp, SERVERPROG, SERVERVERS, serverprog_1, IPPROTO_UDP)) {
			fprintf (stderr, "%s", "unable to register (SERVERPROG, SERVERVERS, udp).");
			exit(1);}
	}

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		fprintf (stderr, "%s", "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, SERVERPROG, SERVERVERS, serverprog_1, IPPROTO_TCP)) {
		fprintf (stderr, "%s", "unable to register (SERVERPROG, SERVERVERS, tcp).");
		exit(1);
	}
	recover_proc();
	timeout_proc2();
	svc_run ();
	fprintf (stderr, "%s", "svc_run returned");
	exit (1);
	/* NOTREACHED */
}
