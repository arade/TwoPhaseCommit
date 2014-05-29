TwoPhaseCommit
==============
By Arade T. Tegen
3/28/2014
Implimenation of 2PC in C
Distributed Systems 2012 – 2013

A Distributed Database System

In this project, simple database system that supports local/distributed transactions is implemented. A server side functions and a corresponding client (test) application has been fully implemented. The programming language used is C with RPC . In the server application in addition to the function dedicated to serving the client, multiple helper function are implemented. The server can handle multiple ongoing transaction. In case the server crush a proper recovery mechanism is implemented. Database is structures as key-value-state where key is a unique identifier , value is any integer value and state is the last state of the system where that field was modified.

Initialization 

[function: void dm_initialize(void)]: whenever we run the server it goes through initialization process.
It will first initialize variables we used in our system. It check if the required database/log-files/directories are in place. If not it will create a new set of files in a new folder named with a format(db_program_number). Then the state of the system will be read from the files. It will then do a recovery process which will be explained later.

Local Transaction Implementation

[function: int * get_local_id_1_svc(struct to_localID *request_LID, struct svc_req *req)]: the process stars by client requesting for a local transaction. This request is answered by the server by giving a local ID and updating the data managers state for the last used local id. 

[function: int * modify_db_1_svc(to_dm *db_mod, struct svc_req *req)]: Using this local ID the client can modify all the key that need modification. This modifications are logged in a temporary file in the server. We also log the state of the database at the time of modification.


[function: int * commit_local_1_svc(commit_struct *decison, struct svc_req *req)]: Finally the client command the server weather to commit or not. If abort the server will clean the temporary files associated with the local ID log the action in a log file. If commit it will first check if the keys have undergone state change after the modification or if any of the keys are in a blocked state (blocked meaning a key is in a process of a commit by other transaction) . If any of the key changes state or is in blocking that local id will automatically aborted. Otherwise it will put all the keys in a blocked state until we finished writing the modification on the database. It also put the local id in the blocking list(we use this in case the system fails and need to do recovery process). When we finalize updating the database, we will remove local id from blocking list and also all the temporary files associated with that local ID.

Distributed Transaction Implementation:

[function: int * get_global_id_1_svc(void *arg, struct svc_req *req)]:The process starts from requesting any of the server for a global ID. The server will give a unique ID and create a recovery list file.(we will call this server a coordinator) . It will update the data managers state for the last used global id.

[Function : int * get_local_id_1_svc(struct to_localID *request_LID, struct svc_req *req)
int * register_dm_1_svc(coord_reg *reg_dm, struct svc_req *req)]: Using this global ID and programing number of the coordinator the client will ask any other data manager(which we will call cohort N.B. it may including the coordinator itself) for a local id. The requested cohort will first register it self on the coordinator. In the coordinator side this request is handled by logging the local id and program number on a recovery list file . The cohort will then return a unique local ID to the client.

[function: int * modify_db_1_svc(to_dm *db_mod, struct svc_req *req)]: Using this local ID the client can modify all the key that need modification. This modification are logged in a temporary file in the server. We also log the state of the database at the time of modification.

[functions: int * commit_global_1_svc(commit_struct *decison, struct svc_req *req)
int * give_vote_1_svc(vote_req *vote_request, struct svc_req *req)
int * commit_local_1_svc(commit_struct *decison, struct svc_req *req)]: Finally the client command the coordinator weather to commit or not. If abort the cohort will simply tell all the involved cohorts to abort. It then clean up temporary files associated with the global id and register the action to a log file. On the cohort side it will also clean the temporary files associated with the local id and log the action in a log file.

If the client request to commit the coordinator will initiate a 2pc. Coordinator ask all the participating cohorts to vote for their respective local id associated with the global transaction. Coordinator send the local id associated with the global transaction and list of {local_id, program number} of all the cohort involved in the transaction for recovery process. The cohort check If any of the key in a given local id is changed state or is in blocking, in that case that cohort will automatically vote aborted. other wise it will put the keys and local id in block list and vote commit.
If the coordinator get an abort from any of the cohorts it will automatically quite the poling and send an abort command to all the cohorts involved in the transaction, clean the temporary files and log the action.

Other wise if all cohorts vote commit it will multi-cast a commit command . The cohorts will modify the database and update the state of the system. It will then clean all the temporary files involved in the transaction and log the action in log file.

Error recovery:

[function : int * decision_state_1_svc(int *local_id, struct svc_req *req)
int * decision_gstate_1_svc(int *global_id, struct svc_req *req)
void recover_proc()
void timeout_proc2()
bool finish_blocked_lid(int id, int type)]: There are two parts to the recovery process . The first one is a recovery process that runs during start up. This error recovery procedure will always try to finalized any local/global id's in a blocked state. For all the transactions whose id is in blocked state full information is stored in a temporary file. So in case a crush happens, when ever we restart the server it will take on from the point it stopped. It will first check for unfinished global ids (ids whose vote collection is finalized) and multi-cast the commit/abort to all the cohort involved. It will the check for unfinished local id's(local id's in blocked state) and ask for the decision from coordinator or any other cohort involved. If any of the above respond the final decision it will finalize, but if the coordinator is not responding and the other cohorts do not know the decision that local ID will enter in to blocking state.

The second recovery process is a periodic process which run in a given interval. during this time it will check if there is any blocked local id and ask for the decision information about that local ID from coordinator and all the cohort involved. If any of the above tell it back the final decision it will finalize but if the coordinator is not responding and the other cohorts do not know the decision it will stay in to blocking state.


Files created:

db_prognumber.dat : is the main database file.

blockingglobal_prognumber.log : is the file which used to stores list of global Ids where all the votes have already been collected but the decision haven't been sent to all the cohorts.

blockinglocal_prognumber.log : is a file which stores list of local Ids which are in blocking state( Ids where its cleared for a commit but haven't been committed yet).

blockinglist_prognumber.log : is a file which stores list of keys which are in blocking state.

db_state_prognumber.dat : is a file which stores the current state of a data manager ( current state, current global ID and current local ID).

global_prognumber.log : is a file which is used to permanently log the action taken on the global id(commit/abort)

local_prognumber.log: is a file which is used to permanently log the action taken on the local id(commit/abort)

tempdb_prognumber_localid.tmp: is a temporary file which is used to hold list of modification made by a clinet Key + value + state of the system during modification. During commit the vale on this file are written on database and this file is deleted.

Dmlistt_prognumber_globalid.tmp : is a temporary file which is used by a coordinator to hold all the information of the cohorts involved in the transaction.{local id + program number}

recoverylist_prognumber_localid.tmp: is the exact copy of Dmlistt_prognumber_globalid.tmp which is multi-casted by the coordinator during vote request. It will be temporarily stored until a cohort properly commits a local id associated with a global transaction.

