#include <unistd.h> //for getopt, optarg
#include <stdio.h>
#include <string.h>//remove warning strcpy
#include <signal.h>//for signals
#include <stdlib.h>
#include <pthread.h> //for pthread functions
#include <dirent.h> //for traversing directories
#include <sys/stat.h> // for lstat, stat, open
#include <fcntl.h> //for O_RDONLY
#include <time.h>
#define ERR(source) (perror(source),\
                     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     exit(EXIT_FAILURE))
#define PATH_L 90
#define OPTION_L 20
#define INDEX_SIZE 50
#define TYPE_L 5
//#define DEBUG
typedef struct index_t
{
    char *file_name; //file name
    char *full_path; //a full (absolute) path to a file
    off_t size; //size
    uid_t st_uid; //owner's uid
    char* type;
} index_t;

typedef struct data_t
{
    index_t*index;
    int places_taken;
    int prev_size;
    char * path_d;
    char *path_f;
    int t;
    int period_re_index;
    int in_progress;
    int can_be_cancelled;
    sigset_t mask;
    pthread_mutex_t *mxindex;
    pthread_mutex_t mxother;
    struct data_t * old_data;

} data_t;


void read_arguments(int argc, char**argv, char * path_d, char *path_f, int *t,int* period_re_index);
void usage();
void get_mole_dir(char *path_d);
void get_mole_index_path(char *path_f);
void option_check(char*arg);
void argument_check(char * path_d, char *path_f, int *t,int* period_re_index);
void* indexing(void *arg);
void traverse_files(void *arg,char *path);
void remove_slash(char*path);
int identify_type(void *arg, char*path);
void save_to_file(void *arg);
int read_from_file(void *arg);
void count_file_type(void *arg);
void get_option_val(char*option,char*option_value);
void get_option(void *arg,pthread_t thread_index);
void sig_handler(int sig);
void sethandler( void (*f)(int), int sigNo);
void print_owner(void*arg,int val);
void print_largerthan(void*arg,int val);
void print_namepart(void *arg, char* option_value);
int is_in_file(void*arg,char*name,char *path);
int is_in_index(void*arg,char*name,char*path);
int check_length(char *string);
void create_data(data_t *data);
void copy_data(data_t *data, data_t *old_data);
void free_data(data_t *data);
void reset_index(data_t* data);
void unlock_all(data_t *data);
int main(int argc, char**argv)
{

    pthread_t thread_index;
    data_t *data=malloc(sizeof(data_t));
    create_data(data);
    data->old_data=malloc(sizeof(data_t));
    create_data(data->old_data);
    read_arguments(argc,argv,data->path_d,data->path_f,&data->t,&data->period_re_index);
#ifdef DEBUG
    argument_check(data->path_d,data->path_f,&data->t,&data->period_re_index);
#endif
//    pthread_attr_t attribute;
//    if(pthread_attr_init(&attribute))
//        ERR("Couldn't initialise attribute");
//    if(pthread_attr_setdetachstate(&attribute,PTHREAD_CREATE_DETACHED))
//        ERR("Couldn't set attribute to detached");
    sigset_t mask, oldmask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);

    if(pthread_sigmask(SIG_BLOCK, &mask, &oldmask))
        ERR("SIGMASk");
    sethandler(sig_handler,SIGUSR1);
    data->mask=mask;
    if(pthread_create(&thread_index,NULL,indexing,data))
        ERR("Couldn't create indexing thread");
    //pthread_detach(thread_index);
//    if(pthread_cancel(thread_index))
//        ERR("Couldn't cancel indexing thread");

    get_option(data,thread_index);

    if(pthread_join(thread_index,NULL))
        ERR("Couldn't join thread");
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    //pthread_attr_destroy(&attribute);
    free_data(data);
    free_data(data->old_data);
    free(data->old_data);
    free(data);
    //pthread_exit(0);
    return 0;
}
void read_arguments(int argc, char**argv, char*  path_d, char *path_f, int *t, int* period_re_index)
{

    int was_f_present=0;
    int was_d_present=0;

    int opt;
    while ((opt = getopt(argc, argv, ":d:f:t:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            option_check(optarg);
            strcpy(path_d,optarg);
            remove_slash(path_d);
            was_d_present=1;
            break;
        case 'f':
            option_check(optarg);
            strcpy(path_f,optarg);
            was_f_present=1;
            break;
        case 't':
            option_check(optarg);
            *period_re_index=1;
            *t = atoi(optarg);
            if(*t<30||*t>7200)
                ERR("Wrong range of t");
            break;
        default:
            usage();
            ERR("Wrong arguments");
        }
    }
    if(!was_d_present)
        get_mole_dir(path_d);

    if(!was_f_present)
        get_mole_index_path(path_f);
}
void get_mole_dir(char *path_d)
{
    setenv("MOLE_DIR","MOLE_DIR",1);
    char *mole_dir=malloc(PATH_L*sizeof(char));
    if(mole_dir==NULL)
        ERR("mole_dir malloc");

    if(snprintf(mole_dir, PATH_L, "%s", getenv("MOLE_DIR")) >= PATH_L)
    {
        ERR("Size of buffer was too small");
    }
    mole_dir =getenv("MOLE_DIR");
    if(mole_dir==NULL)
    {
        ERR("Couldn't retrieve value from MOLE_DIR");
    }
    else
        strcpy(path_d,mole_dir);
    //free(mole_dir);
}
void get_mole_index_path(char *path_f)
{
    setenv("MOLE_INDEX_PATH","MOLE_INDEX_PATH",1);
    char *mole_index_path=malloc(PATH_L*sizeof(char));
    if(mole_index_path==NULL)
        ERR("mole_index_path malloc");
    if(snprintf(mole_index_path, PATH_L, "%s", getenv("MOLE_INDEX_PATH")) >= PATH_L)
    {
        ERR("Size of buffer was too small");
    }
    mole_index_path=getenv("MOLE_INDEX_PATH");
    if(mole_index_path==NULL)
    {
        FILE *fp;
        mole_index_path=calloc(PATH_L,sizeof(char));
        fp=fopen(".mole-index","r");
        if(fp==NULL)
            ERR("Couldnt open the file .mole-index");
        fgets(mole_index_path,PATH_L,fp);
        fclose(fp);
    }
    strcpy(path_f,mole_index_path);
   // free(mole_index_path);
}
void usage()
{
    fprintf(stderr, "Usage: [-d path] [-f path] [-t n]\nn is from range [30,7200]");
}
void option_check(char*arg)
{
    if(0==strcmp(arg," -f")||0==strcmp(arg," -t")||0==strcmp(arg," -d")||
            0==strcmp(arg,"-f")||0==strcmp(arg,"-t")||0==strcmp(arg,"-d"))
    {
        usage();
        ERR("Empty argument");
    }
}
void argument_check(char * path_d, char *path_f, int *t,int* period_re_index)
{
    printf("------LOADED VALUES-------\n");
    if(*period_re_index)
    {
        printf("periodic re-indexing enabled value:%d\n",*t);
    }
    else
    {
        printf("periodic re-indexing disabled\n");
    }
    printf("path_d:%s\n",path_d);
    printf("path_f:%s\n",path_f);
    printf("--------------------------\n");
}
void* indexing(void* arg)
{
    //pthread_detach(pthread_self());
    data_t *data=arg;
    int signo=0;
    struct stat st;
    struct timespec tt_new;
    pthread_mutex_lock(&data->mxother);
    struct timespec currtime;
    clock_gettime(CLOCK_REALTIME, &currtime);

    if(stat(data->path_f,&st)==0)
    {
        tt_new.tv_sec=data->t-(currtime.tv_sec-st.st_ctim.tv_sec);
        //printf("%d\n", currtime.tv_sec-st.st_ctim.tv_sec);
    }
    else{
        tt_new.tv_sec=data->t;}
    tt_new.tv_nsec=0;
    if(tt_new.tv_sec<0)
        tt_new.tv_sec=0;
    pthread_mutex_unlock(&data->mxother);
    if(!read_from_file(arg))
    {

        pthread_mutex_lock(&data->mxother);
        data->in_progress=1;
        pthread_mutex_unlock(&data->mxother);
        data->places_taken=0;
        //reset_index(data);
        unlock_all(data);
        traverse_files(arg,data->path_d);

        save_to_file(arg);

        copy_data(data,data->old_data);

        printf("Finished indexing...\n");
        pthread_mutex_lock(&data->mxother);
        data->in_progress=0;
        pthread_mutex_unlock(&data->mxother);
    }
    while(1)
    {
        pthread_mutex_lock(&data->mxother);
        if(data->period_re_index)
        {
            nanosleep(&tt_new,NULL);
            tt_new.tv_sec=data->t;

            data->in_progress=1;
            pthread_mutex_unlock(&data->mxother);
            data->places_taken=0;

            //reset_index(data);
            unlock_all(data);
            traverse_files(arg,data->path_d);


            save_to_file(arg);

            copy_data(data,data->old_data);

            printf("Finished indexing...\n");
            pthread_mutex_lock(&data->mxother);
            data->in_progress=0;
            pthread_mutex_unlock(&data->mxother);
        }
        else
        {

            sigwait(&data->mask,&signo);
            if(signo==SIGUSR2)
            {

                data->in_progress=1;
                pthread_mutex_unlock(&data->mxother);
                data->places_taken=0;
                //reset_index(data);

                unlock_all(data);
                traverse_files(arg,data->path_d);

                save_to_file(arg);
                copy_data(data,data->old_data);

                printf("Finished indexing...\n");
                pthread_mutex_lock(&data->mxother);
                data->in_progress=0;
                pthread_mutex_unlock(&data->mxother);
            }

        }
    }
    return NULL;

}
void traverse_files(void *arg,char *given_path)
{
    char *path=calloc(2*PATH_L,sizeof(char));
    struct stat sb;
    if(NULL==path)
        ERR("Path calloc");
    data_t *data=arg;
    DIR *directory;
    struct dirent *dir_entry;
    directory=opendir(given_path);
    if(NULL==directory)
    {

        free(path);
        return;
    }
    while((dir_entry=readdir(directory))!=NULL)
    {

        if(strcmp(dir_entry->d_name,".")!=0&&strcmp(dir_entry->d_name,"..")!=0)
        {

            strcpy(path,given_path);
            strcat(path,"/");
            strcat(path,dir_entry->d_name);
            strcat(path,"\0");

            if(check_length(path))
            {
                printf("Absolute path: %s of file was too long (longer than %d)\n",path,PATH_L);

                continue;
            }

            lstat(path,&sb);

            //if(!is_in_index(arg,dir_entry->d_name,path))
            //{

            if(dir_entry->d_type==DT_DIR)
            {

                pthread_mutex_lock(&data->mxindex[data->places_taken]);

                strcpy(data->index[data->places_taken].type,"DIR\0");
                pthread_mutex_unlock(&data->mxindex[data->places_taken]);

            }
            if(dir_entry->d_type==DT_DIR||!identify_type(arg,path))
            {
                pthread_mutex_lock(&data->mxindex[data->places_taken]);
                strcpy(data->index[data->places_taken].file_name,dir_entry->d_name);
                strcat(data->index[data->places_taken].file_name,"\0");
                strcpy(data->index[data->places_taken].full_path,path);
                data->index[data->places_taken].st_uid=sb.st_uid;
                data->index[data->places_taken].size=sb.st_size;
                pthread_mutex_unlock(&data->mxindex[data->places_taken]);
                pthread_mutex_lock(&data->mxother);
                data->places_taken++;
                pthread_mutex_unlock(&data->mxother);
            }
            // }

            traverse_files(arg,path);
        }
    }
    if(closedir(directory))
        ERR("Couldnt close directory");
    free(path);
}
void remove_slash(char*path)
{
    int prev=0;
    for(int i=1; i<PATH_L; i++)
    {
        if(path[i]=='\0'&&path[prev]=='/')
            path[prev]='\0';
        prev=i;
    }
}
int identify_type(void *arg, char*path)
{
    if(arg==NULL)
        return 1;
    data_t *data=arg;
    int file_descr;
    char *magic_numbers=malloc(8*sizeof(char));
    if(magic_numbers==NULL)
        ERR("magic_numbers malloc");
    if((file_descr=open(path,O_RDONLY))>=0)
    {
        pthread_mutex_lock(&data->mxindex[data->places_taken]);
        int check=read(file_descr,magic_numbers,8);
        if(-1==check)
            ERR("Couldn't file signature");
        else if(0==check)
        {
            pthread_mutex_unlock(&data->mxindex[data->places_taken]);
            close(file_descr);
            free(magic_numbers);

            return 1;


        }

        if(magic_numbers[0]==80&&magic_numbers[1]==75&&((magic_numbers[2]==3&&magic_numbers[3]==4)||(magic_numbers[2]==5&&magic_numbers[3]==6)||(magic_numbers[2]==7&&magic_numbers[3]==8)))
        {
            strcpy(data->index[data->places_taken].type,"ZIP\0");
            close(file_descr);
            free(magic_numbers);
            pthread_mutex_unlock(&data->mxindex[data->places_taken]);
            return 0;
        }
        else if(magic_numbers[0]==31&&magic_numbers[1]==-117)
        {
            strcpy(data->index[data->places_taken].type,"GZIP\0");
            close(file_descr);
            free(magic_numbers);
            pthread_mutex_unlock(&data->mxindex[data->places_taken]);
            return 0;
        }
        else if(magic_numbers[0]==-119&&magic_numbers[1]==80&&magic_numbers[2]==78&&magic_numbers[3]==71&&magic_numbers[4]==13&&magic_numbers[5]==10&&magic_numbers[6]==26&&magic_numbers[7]==10)
        {
            strcpy(data->index[data->places_taken].type,"PNG\0");
            close(file_descr);
            free(magic_numbers);
            pthread_mutex_unlock(&data->mxindex[data->places_taken]);
            return 0;
        }
        else if(magic_numbers[0]==-1&&magic_numbers[1]==-40&&magic_numbers[2]==-1&&magic_numbers[3]==-32)
        {
            strcpy(data->index[data->places_taken].type,"JPEG\0");
            close(file_descr);
            free(magic_numbers);
            pthread_mutex_unlock(&data->mxindex[data->places_taken]);
            return 0;
        }
        else
        {
            close(file_descr);
            free(magic_numbers);
            pthread_mutex_unlock(&data->mxindex[data->places_taken]);
            return 1;
        }
    }
    else
    {
        close(file_descr);
        free(magic_numbers);
        pthread_mutex_unlock(&data->mxindex[data->places_taken]);
        return 1;
    }

}
void save_to_file(void *arg)
{
    if(arg==NULL)
        return;
    data_t *data=arg;
    data->can_be_cancelled=0;
    int file_descriptor;
    file_descriptor=open(data->path_f,O_WRONLY|O_TRUNC|O_CREAT,0644);
    if(file_descriptor==-1)
        ERR("couldn't open the file");
    for(int i=0; i<data->places_taken; i++)
    {
        //pthread_mutex_lock(&data->mxindex[i]);
       // if(data->index[i].file_name[0]==0)
       // {
            //pthread_mutex_unlock(&data->mxindex[i]);
            //break;
        //}
        if(data->index[i].file_name==NULL||data->index[i].full_path==NULL||data->index[i].type==NULL)
            continue;


        // pthread_mutex_unlock(&data->mxindex[i]);
        if(is_in_file(arg,data->index[i].file_name,data->index[i].full_path))
        {
            pthread_mutex_lock(&data->mxindex[i]);
            if(-1==write(file_descriptor,data->index[i].file_name,PATH_L))
                ERR("write to index");
            if(-1==write(file_descriptor,data->index[i].full_path,PATH_L))
                ERR("Write to index");
            if(-1==write(file_descriptor,&(data->index[i].size),sizeof(off_t)))
                ERR("Write to index");
            if(-1==write(file_descriptor,&(data->index[i].st_uid),sizeof(uid_t)))
                ERR("Write to index");
            if(-1==write(file_descriptor,data->index[i].type,TYPE_L))
                ERR("Write to index");
            pthread_mutex_unlock(&data->mxindex[i]);
        }
        //pthread_mutex_unlock(&data->mxindex[i]);
    }

    close(file_descriptor);
    data->can_be_cancelled=1;
}
int read_from_file(void *arg)
{
    data_t *data=arg;
    int file_descriptor;
    file_descriptor=open(data->path_f,O_RDONLY,0);

    if(file_descriptor==-1)
        return 0;

    char * name_path=malloc(PATH_L*sizeof(char));
    off_t size;
    uid_t uid;
    char*type=malloc(TYPE_L);
    int result;
    int i=0;
    while(1)
    {
        pthread_mutex_lock(&data->mxother);
        pthread_mutex_lock(&data->mxindex[data->places_taken]);

        if(i>=INDEX_SIZE)
        {
            ERR("INDEX_SIZE IS TOO SMALL");
            break;

        }
        result=read(file_descriptor,name_path,PATH_L);
        if(0==result)
        {
            pthread_mutex_unlock(&data->mxother);
            pthread_mutex_unlock(&data->mxindex[data->places_taken]);
            break;
        }
        else if(-1==result)
            ERR("Read index");
        strcpy(data->index[data->places_taken].file_name,name_path);
        if(-1==read(file_descriptor,name_path,PATH_L))
            ERR("Read index");
        strcpy(data->index[data->places_taken].full_path,name_path);
        if(-1==read(file_descriptor,&size,sizeof(off_t)))
            ERR("Read index");
        data->index[data->places_taken].size=size;
        if(-1==read(file_descriptor,&uid,sizeof(uid_t)))
            ERR("Read index");
        data->index[data->places_taken].st_uid=uid;
        if(-1==read(file_descriptor,type,TYPE_L))
            ERR("Read index");
        strcpy(data->index[data->places_taken].type,type);
        i++;
        data->places_taken++;
        pthread_mutex_unlock(&data->mxother);
        pthread_mutex_unlock(&data->mxindex[data->places_taken]);
    }
    close(file_descriptor);
    free(name_path);
    free(type);
    return 1;
}
void count_file_type(void *arg)
{
    data_t *data=arg;
    int jpeg=0;
    int dir=0;
    int zip=0;
    int png=0;
    int gzip=0;
    for(int i=0; i<data->places_taken; i++)
    {
        //pthread_mutex_lock(&data->mxindex[i]);
        if(data->index[i].file_name[0]==0)
        {
            //pthread_mutex_unlock(&data->mxindex[i]);
            break;
        }
        if(!strncmp(data->index[i].type,"DIR",3))
        {
            dir++;
        }
        else if(!strncmp(data->index[i].type,"PNG",3))
        {
            png++;
        }
        else if(!strncmp(data->index[i].type,"ZIP",3))
        {
            zip++;
        }
        else if(!strncmp(data->index[i].type,"JPEG",4))
        {
            jpeg++;
        }
        else if(!strncmp(data->index[i].type,"GZIP",4))
        {
            gzip++;
        }
        //pthread_mutex_unlock(&data->mxindex[i]);
    }
    printf("DIR: %d\n",dir);
    printf("ZIP: %d\n",zip);
    printf("GZIP: %d\n",gzip);
    printf("PNG: %d\n",png);
    printf("JPEG: %d\n",jpeg);
}
void get_option_val(char*option,char*option_value)
{
    int space=0;
    for(int i=0; i<OPTION_L; i++)
    {
        if(option[i]==' ')
        {
            space=i;
            break;
        }
    }
    int j=0;
    for(int i=space+1; i<OPTION_L; i++)
    {
        if(option[i]=='\0'||option[i]=='\n')
        {
            option_value[j]='\0';
            break;
        }
        option_value[j]=option[i];
        j++;
    }
}
void get_option(void *arg, pthread_t thread_index)
{

    data_t*data=arg;
    char *option =malloc(OPTION_L);
    if(option==NULL)
        ERR("Option malloc");
    char *option_value=malloc(OPTION_L);
    if(option_value==NULL)
        ERR("Option_value malloc");
    struct timespec tt= {0,5000};
    while(1)
    {

        fgets(option,OPTION_L,stdin);

        if(!strncmp(option,"exit\n",5))
        {
            printf("exiting...\n");
            while(data->in_progress==1)
                nanosleep(&tt,NULL);
            //pthread_detach(thread_index);
            if(pthread_cancel(thread_index))
                ERR("Couldn't cancel thread");
            break;
        }
        else if(!strncmp(option,"exit!\n",6))
        {
            printf("Exiting!...\n");
            while(data->can_be_cancelled==0)
                nanosleep(&tt,NULL);
//pthread_detach(thread_index);
            if(pthread_cancel(thread_index))
                ERR("Couldn't cancel thread");
            break;
        }
        else if(!strncmp(option,"index\n",6))
        {
            if(data->in_progress==0)
            {

                if(data->period_re_index)
                    pthread_kill(thread_index,SIGUSR1);
                else
                    pthread_kill(thread_index,SIGUSR2);
            }
            else
            {
                printf("Indexing is already in progess\n");

            }
        }
        else if(!strncmp(option,"count\n",6))
        {
            if(data->in_progress==0)
                count_file_type(data);
            else
                count_file_type(data->old_data);
        }
        else if(!strncmp(option,"largerthan ",11))
        {
            get_option_val(option,option_value);
            int val=atoi(option_value);
            if(data->in_progress==0)
                print_largerthan(arg,val);
            else
                print_largerthan(data->old_data,val);

        }
        else if(!strncmp(option,"namepart ",9))
        {
            get_option_val(option,option_value);
            if(data->in_progress==0)
                print_namepart(arg,option_value);
            else
                print_namepart(data->old_data,option_value);
        }
        else if(!strncmp(option,"owner ",6))
        {

            get_option_val(option,option_value);
            int val=atoi(option_value);
            if(data->in_progress==0)
                print_owner(arg,val);
            else
                print_owner(data->old_data,val);
        }
        else
        {
            fprintf(stderr,"Incorrect option!\n");
        }

    }
    free(option);
    free(option_value);
}
void sig_handler(int sig)
{
    return;
}
void sethandler( void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1==sigaction(sigNo, &act, NULL)) ERR("sigaction");
}
void print_owner(void*arg,int val)
{
    data_t *data=arg;
    int howmany=0;
    for(int i=0; i<data->places_taken; i++)
    {
        // pthread_mutex_lock(&data->mxindex[i]);
        if(data->index[i].st_uid==val)
        {
            howmany++;
        }
        // pthread_mutex_unlock(&data->mxindex[i]);
    }
    if(howmany>=3)
    {
        FILE *fp;
        fp=popen("less","w");
        if(fp==NULL)
            ERR("popen");
        for(int i=0; i<data->places_taken; i++)
        {
            //pthread_mutex_lock(&data->mxindex[i]);
            if(data->index[i].st_uid==val)
            {
                fprintf(fp,"%s\n",data->index[i].full_path);
                fprintf(fp,"%ld\n",data->index[i].size);
                fprintf(fp,"%s\n",data->index[i].type);
            }
            //pthread_mutex_unlock(&data->mxindex[i]);
        }

        pclose(fp);

    }
    else
    {
        printf("-------files with owener uid == %d---------\n",val);
        for(int i=0; i<data->places_taken; i++)
        {
            //pthread_mutex_lock(&data->mxindex[i]);
            if(data->index[i].st_uid==val)
            {
                printf("%s\n",data->index[i].full_path);
                printf("%ld\n",data->index[i].size);
                printf("%s\n",data->index[i].type);
            }
            //pthread_mutex_unlock(&data->mxindex[i]);
        }
    }
}
void print_largerthan(void*arg,int val)
{

    data_t *data=arg;
    int howmany=0;
    for(int i=0; i<data->places_taken; i++)
    {
        //pthread_mutex_lock(&data->mxindex[i]);
        if(data->index[i].size>val)
        {
            howmany++;
        }
        //pthread_mutex_unlock(&data->mxindex[i]);
    }
    if(howmany>=3)
    {
        FILE *fp;
        fp=popen("less","w");
        if(fp==NULL)
            ERR("popen");
        for(int i=0; i<data->places_taken; i++)
        {
            // pthread_mutex_lock(&data->mxindex[i]);
            if(data->index[i].size>val)
            {
                fprintf(fp,"%s\n",data->index[i].full_path);
                fprintf(fp,"%ld\n",data->index[i].size);
                fprintf(fp,"%s\n",data->index[i].type);
            }
            // pthread_mutex_unlock(&data->mxindex[i]);
        }

        pclose(fp);

    }
    else
    {
        printf("-------files with size > %d---------\n",val);
        for(int i=0; i<data->places_taken; i++)
        {
            //pthread_mutex_lock(&data->mxindex[i]);
            if(data->index[i].size>val)
            {
                printf("%s\n",data->index[i].full_path);
                printf("%ld\n",data->index[i].size);
                printf("%s\n",data->index[i].type);
            }
            //pthread_mutex_unlock(&data->mxindex[i]);
        }
    }
}
void print_namepart(void *arg, char* option_value)
{
    data_t *data=arg;
    int howmany=0;
    for(int i=0; i<data->places_taken; i++)
    {
        //pthread_mutex_lock(&data->mxindex[i]);

        if(strstr(data->index[i].file_name,option_value)!=NULL)
        {
            howmany++;
        }
        //pthread_mutex_unlock(&data->mxindex[i]);
    }
    if(howmany>=3)
    {
        FILE *fp;
        fp=popen("less","w");
        if(fp==NULL)
            ERR("popen");
        for(int i=0; i<data->places_taken; i++)
        {
            //pthread_mutex_lock(&data->mxindex[i]);
            if(strstr(data->index[i].file_name,option_value)!=NULL)
            {

                fprintf(fp,"%s\n",data->index[i].full_path);
                fprintf(fp,"%ld\n",data->index[i].size);
                fprintf(fp,"%s\n",data->index[i].type);

            }
            //pthread_mutex_unlock(&data->mxindex[i]);
        }

        pclose(fp);

    }
    else
    {
        for(int i=0; i<data->places_taken; i++)
        {
            //pthread_mutex_lock(&data->mxindex[i]);
            if(strstr(data->index[i].file_name,option_value)!=NULL)
            {
                printf("%s\n",data->index[i].full_path);
                printf("%ld\n",data->index[i].size);
                printf("%s\n",data->index[i].type);
            }
            //pthread_mutex_unlock(&data->mxindex[i]);
        }
    }

}
int is_in_file(void*arg,char*name,char*path)
{
   if(name==NULL||path==NULL||arg==NULL)
        return 0;
    int index=0;
    int index2=0;
    for(int i=0; i<PATH_L; i++)
    {
        if(name[i]=='\0')
        {
            index=i+1;
            break;
        }
    }
    for(int i=0; i<PATH_L; i++)
    {
        if(path[i]=='\0')
        {
            index2=i+1;
            break;
        }
    }
    data_t *data=arg;
    int file_descriptor;
    file_descriptor=open(data->path_f,O_RDONLY|O_CREAT,0644);
    if(file_descriptor==-1)
        ERR("Couldn't open the file");
    char * temp=malloc(PATH_L*sizeof(char));
    if(temp==NULL)
        ERR("temp malloc error");
    char * temp2=malloc(PATH_L*sizeof(char));
        if(temp2==NULL)
        ERR("temp malloc error");
    off_t temp3;
    uid_t temp4;
    char*temp5=malloc(TYPE_L);
            if(temp5==NULL)
        ERR("temp malloc error");
    for(int i=0; i<data->places_taken; i++)
    {

        if(data->index[i].file_name[0]==0)
        {
            break;
        }
        if(-1==read(file_descriptor,temp,PATH_L))
            ERR("Read index");

        if(-1==read(file_descriptor,temp2,PATH_L))
            ERR("Read index");
        if(!strncmp(name,temp,index)&&!strncmp(path,temp2,index2))
        {
            close(file_descriptor);
            free(temp);
            free(temp2);
            free(temp5);
            return 0;
        }

        if(-1==read(file_descriptor,&temp3,sizeof(off_t)))
            ERR("Read index");

        if(-1==read(file_descriptor,&temp4,sizeof(uid_t)))
            ERR("Read index");

        if(-1==read(file_descriptor,temp5,TYPE_L))
            ERR("Read index");

    }
    close(file_descriptor);
    free(temp);
    free(temp2);
    free(temp5);
    return 1;
}
int is_in_index(void*arg,char*name,char*path)
{
    data_t *data=arg;
    for(int i=0; i<data->places_taken; i++)
    {
        //pthread_mutex_lock(&data->mxindex[i]);

        if(!strcmp(data->index[i].full_path,path))
        {
            // pthread_mutex_unlock(&data->mxindex[i]);
            return 1;
        }
        //pthread_mutex_unlock(&data->mxindex[i]);
    }
    return 0;
}
int check_length(char *string)
{
    int size=0;
    while(string[size]!='\0')
    {
        if(size>=PATH_L)
            return 1;
        size++;
    }
    return 0;
}
void create_data(data_t *data)
{
    if(data==NULL)
        ERR("data malloc");
    data->places_taken=0;
    data->path_d=calloc(PATH_L,sizeof(char));
    if(data->path_d==NULL)
        ERR("Path_d calloc");
    data->path_f=calloc(PATH_L,sizeof(char));
    if(data->path_f==NULL)
        ERR("Path_f calloc");
    data->index=malloc(INDEX_SIZE*sizeof(index_t));
    if(data->index==NULL)
        ERR("index malloc");
    data->in_progress=0;
    data->can_be_cancelled=1;
    data->mxindex=malloc(INDEX_SIZE*sizeof(pthread_mutex_t));
    if(data->mxindex==NULL)
        ERR("mxindex malloc");
    for(int i=0; i<INDEX_SIZE; i++)
    {
        data->index[i].type=malloc(TYPE_L*sizeof(char));
        if(data->index[i].type==NULL)
            ERR("type malloc");
        data->index[i].file_name=calloc(PATH_L,sizeof(char));
        if(data->index[i].file_name==NULL)
            ERR("index file name malloc");
        data->index[i].full_path=malloc(PATH_L*sizeof(char));
        if(data->index[i].full_path==NULL)
            ERR("index file name malloc");
        if(pthread_mutex_init(&data->mxindex[i],NULL))
            ERR("mxindex init");
    }
    if(pthread_mutex_init(&data->mxother,NULL))
        ERR("mxother init");
}
void copy_data(data_t *data, data_t *old_data)
{
//    pthread_mutex_lock(&data->mxother);
    //  pthread_mutex_lock(&old_data->mxother);
    old_data->places_taken=data->places_taken;
    strcpy(old_data->path_d,data->path_d);
    strcpy(old_data->path_f,data->path_f);
    old_data->in_progress=data->in_progress;
    old_data->can_be_cancelled=data->can_be_cancelled;
    old_data->mask=data->mask;
    old_data->period_re_index=data->period_re_index;
    old_data->t=data->t;
    old_data->prev_size=data->prev_size;
//   pthread_mutex_unlock(&old_data->mxother);
//    pthread_mutex_unlock(&data->mxother);
    for(int i=0; i<data->places_taken; i++)
    {
        // pthread_mutex_lock(&data->mxindex[i]);
        pthread_mutex_lock(&old_data->mxindex[i]);
        strcpy(old_data->index[i].type,data->index[i].type);
        strcpy(old_data->index[i].file_name,data->index[i].file_name);
        strcpy(old_data->index[i].full_path,data->index[i].full_path);
        old_data->index[i].size=data->index[i].size;
        old_data->index[i].st_uid=data->index[i].st_uid;
        pthread_mutex_unlock(&old_data->mxindex[i]);
        //pthread_mutex_unlock(&data->mxindex[i]);
    }
}
void print_paths(void *arg)
{
    data_t *data=arg;
    for(int i=0; i<INDEX_SIZE; i++)
    {
        printf("%s\n",data->index[i].full_path);
    }
}
void free_data(data_t *data)
{
    for(int i=0; i<INDEX_SIZE; i++)
    {
        free(data->index[i].file_name);
        free(data->index[i].full_path);
        free(data->index[i].type);
        pthread_mutex_destroy(&data->mxindex[i]);

    }
    pthread_mutex_destroy(&data->mxother);
    //pthread_mutex_destroy(data->mxindex);
    free(data->mxindex);
    free(data->index);
    free(data->path_d);
    free(data->path_f);
}
void reset_index(data_t* data)
{
    for(int i=0; i<INDEX_SIZE; i++)
    {
        free(data->index[i].file_name);
        free(data->index[i].full_path);
        free(data->index[i].type);

    }
    data->places_taken=0;
    data->can_be_cancelled=1;
    for(int i=0; i<INDEX_SIZE; i++)
    {
        data->index[i].type=malloc(TYPE_L*sizeof(char));
        if(data->index[i].type==NULL)
            ERR("type malloc");
        data->index[i].file_name=calloc(PATH_L,sizeof(char));
        if(data->index[i].file_name==NULL)
            ERR("index file name malloc");
        data->index[i].full_path=malloc(PATH_L*sizeof(char));
        if(data->index[i].full_path==NULL)
            ERR("index file name malloc");
    }
}
void unlock_all(data_t *data)
{

    for(int i=0; i<INDEX_SIZE; i++)
    {
        pthread_mutex_unlock(&data->mxindex[i]);
    }
}
