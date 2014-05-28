/*
 * Client side application for Simple Distributed Transaction
 * Written By Arade T Tegen
 * 
 */

#include <memory.h> /* for memset */
#include "./server.h"
#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include <rpc/rpc.h>
#include<stdbool.h>


char static IP[20];

int clean_stdin()
{
    while (getchar()!='\n');
    return 1;
}

int main(int argc, char **argv) 
{
	//Here we define variables we will use throughout our program
	int trans_type,prognum,prognum_g,decision,option,x;
	memset(IP, 0, 20);
	strcat(IP, argv[1]);// the IP adress of our DM
	//IP = argv[1]; // the IP adress of our DM
	CLIENT *global,*local;	
	int *global_id,*local_id;
	struct to_localID request_localID;//the data sent to ask  a DM for a local_id
	char *arg;
	char dm_con,repeat_mod,global_ip,local_ip,c;
			
//********************************************************************************************************************************	
	
	//here we choose the type of transaction we want
	printf("\nEnter '0' for Local Transaction, '1' for Distributed Transaction or anyother key to exit: ");
 	if (((scanf("%d%c", &option, &c)!=2 || c!='\n') && clean_stdin())|| option>1 || option<0)
 		exit(0);	
	if (option == 0){
		request_localID.trans_type=0;
		request_localID.global_id=0;
		request_localID.global_prognum=0;
		}//end of if
	else{
		//if we need a distributed transaction here we ask for global ID
global_ip:	printf("Please enter the program number of the Coordinator you want to connect: ");
		scanf("%d",&prognum_g);
		global = clnt_create(IP,prognum_g,SERVERVERS, "udp");
		if (global == NULL) {
			clnt_pcreateerror(IP);
			printf("Try again! ");
			goto global_ip;
			}
		global_id = get_global_id_1((void*)&arg,global);
		if(*global_id){
			request_localID.trans_type=1;
			request_localID.global_id=*global_id;
			request_localID.global_prognum=prognum_g;
			}
		else{
			printf("Can't get global ID\n");
			printf("Try again! ");
			goto global_ip;
			}
		clnt_destroy (global);
		}//end of else

//********************************************************************************************************************************	
		//Here we connect to the DM for a local ID	
dm_con: 	printf("Please enter the program number of the DM you want to connect to:  ");
		scanf("%d",&prognum);	
		local = clnt_create(IP,prognum,SERVERVERS, "udp");
		if (local == NULL) {
			clnt_pcreateerror(IP);
			printf(" Try again! ");
			goto dm_con;
			}
			
		local_id = get_local_id_1(&request_localID,local);
		//here we give the data we want to modify to the DM
		if(*local_id){
			struct to_dm db_mod;
			int key,value;			
			do{
				printf("Please Enter the key to be modified:\n");
				do {  
					printf("Enter an integer from 1 to 999: ");
				    	} while (((scanf("%d%c", &key, &c)!=2 || c!='\n') && clean_stdin()) || key<1 || key>1000);
			
				printf("Please Enter the Value to be modified:\n");
				do {  
					printf("Enter an Integer value only: ");
				    	} while (((scanf("%d%c", &value, &c)!=2 || c!='\n') && clean_stdin()));			 

				db_mod.key= key;		 
				db_mod.value= value;
				db_mod.local_id=*local_id;
				modify_db_1(&db_mod,local);
				printf("Press '1' to continue modifying other key using this local ID, to exit enter any other key: ");
				}while (((scanf("%d%c", &x, &c)!=2 || c!='\n') && clean_stdin())|| x==1);
			}
		else{
			printf("Can't get local ID\n");
			printf("Try again! ");
			goto dm_con;
			}		
		//if we are dealing with global transaction we loop the above action 
		if(option == 1){
			clnt_destroy (local);//we first stop the connection with the privious DM
			printf("Press '1' to continue modifying on other DM's, to exit enter anyother key: ");
			while (((scanf("%d%c", &x, &c)!=2 || c!='\n') && clean_stdin())|| x==1)
				goto dm_con;	
			}

//********************************************************************************************************************************	
//here we do the final commit desion
	struct commit_struct decison;
	decison.commit=false;
	int y;
	char tmp[10];	
	printf("Press '1' to commit, Any other number to discard the transaction: ");
	if(((scanf("%d%c", &decision, &c)!=2 || c!='\n') && clean_stdin()))		
		decision=0;
	if(option == 1){
		decison.type=1;
		global = clnt_create(IP,prognum_g,SERVERVERS, "udp");
		if (global == NULL) {
			clnt_pcreateerror(IP);
			printf("Can't connect\n");
			}	
		decison.id=*global_id;
		
		if(decision==1)
			decison.commit=true;
		y=*commit_global_1(&decison,global);
		printf("action taken %d\n",y);
		if(y==0)
			printf("Aborted\n");
		else
			printf("Commited\n");
		clnt_destroy (global);
		}
	else	{
		decison.type=0;
		local = clnt_create(IP,prognum,SERVERVERS, "udp");
		if (local == NULL) {
			clnt_pcreateerror(IP);
			printf("Can't connect\n");
			exit;
			}
		decison.id=*local_id;
		if(decision==1)
			decison.commit=true;
			
		y=*commit_local_1(&decison,local);
		printf("action taken %d\n",y);
		if(y==0)
			printf("Aborted\n");
		else
			printf("Commited\n");
		clnt_destroy (local);
		}
	
}	//end of main

//***************************************************************************************
//***************************************************************************************

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
