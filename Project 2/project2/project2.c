/*

 * compling: gcc project2.c -o project2 -Wall -ansi -W -std=c99 -g -ggdb -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -lfuse -lmagic -lansilove -pedantic
 * run:  ./project2 -d src dst
 *
 */



#define FUSE_USE_VERSION 26
#define MIME_DB "/usr/share/file/magic.mgc"

static const char* rofsVersion = "2019.12.09";  

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <unistd.h>
#include <fuse.h>
#include <magic.h>
#include <ansilove.h>

// Global to store our read-write path
char *rw_path;


static char* find_original_path(char *path){
	
	printf("find_original_path: %s\n",path);
	
	if(strlen(path)>4){
		char *temp = (char*)malloc(sizeof(char)*3);
		strcpy(temp,path+(strlen(path)-3));
		printf("karakter %s\n",path+(strlen(path)-3));
		if(strcmp(temp,"png")!=0){
			free(temp);
			return NULL;
		}
	}else{
		printf("less size 4\n");
		return NULL;
	}
	
	char *filename = NULL;
	char *file_ex = NULL;
	char *new_path = NULL;
	
	int i= strlen(path)-1;
	for(;i>=0;i--){
		if(path[i]=='/'){
			if(i==((int)strlen(path)-1)){
				return NULL;
			}
			filename = (char *)malloc(sizeof(char)*strlen(path)-i);
			strcpy(filename, path+i+1);
			filename[strlen(filename)]='\0';
			printf("filename: %s\n",filename);
			break;
		}
	}
	
	new_path = (char *)malloc(sizeof(char)*(strlen(path)+10));
	strncpy(new_path, path, i);
	new_path[i]='/';
	new_path[i+1]='\0';
	printf("calculated new_path: %s\n",new_path);
	
	file_ex = (char *)malloc(sizeof(char)*(strlen(filename)-3));
	strncpy(file_ex,filename,strlen(filename)-4);
	file_ex[strlen(filename)-4]='\0';
	printf("file_ex: %s\n", file_ex);

	struct dirent *dp;
    DIR *dir = opendir(new_path);

    // Unable to open directory stream
    if (!dir) {
		printf("find_original_path Returned NUll\n");
		free(filename);
		free(file_ex);
		free(new_path);
        return NULL;
	} 
	
	int flag=0;
    while ((dp = readdir(dir)) != NULL)
    {
		
		if(strncmp(file_ex,dp->d_name,strlen(file_ex))==0){
			printf("match dp->d_name: %s\n", dp->d_name);
			flag=1;
			strcat(new_path,dp->d_name);
			new_path[strlen(new_path)]='\0';
			printf("last new_path: %s\n", new_path);
		}
        
    }

    // Close directory stream
    
    free(filename);
	free(file_ex);
	//free(new_path);
	
    closedir(dir);
    
    if(flag){
		return new_path;
	}else{
		free(new_path);
		return NULL;
	}		
}
static char* ansi2png(char *filename, int *len){
	
	printf("ansi2png filename: %s\n",filename);
	
    struct ansilove_ctx ctx;
	struct ansilove_options options;
	
    ansilove_init(&ctx, &options);

	ansilove_loadfile(&ctx, filename);

	ansilove_ansi(&ctx, &options);

	//ansilove_savefile(&ctx, "example.png");
	*len = ctx.png.length;
	char *buffer=(char *)malloc((*len)*sizeof(char));
	memcpy(buffer, ctx.png.buffer,*len);

	ansilove_clean(&ctx);
	
	return buffer;
}

static int magic_checker(char *filename){
		printf("magic_checker filename: %s\n",filename);
        const char *mimetype;
        magic_t magic_cookie;
        
        magic_cookie=magic_open(MAGIC_MIME_TYPE);
        
        if(magic_cookie  == NULL){
                printf("error creating magic cookie\n");
                return 1;
        }
        
        magic_load(magic_cookie,MIME_DB);
        mimetype=magic_file(magic_cookie,filename);
        printf("magic_checker filetype:%s\n",mimetype);

        
        if(strncmp(mimetype,"application/octet-stream",strlen("application/octet-stream"))==0){
			printf("application/octet-stream\n");
			return 0;
		}
		else if(strncmp("text/", mimetype, strlen("text/"))==0){
			printf("text/plain\n");
			return 0;
		}/*
		else if(strncmp(mimetype,"text/html",strlen("text/html"))==0){
			printf("text/html\n");
			return 0;
		}
		else if(strncmp(mimetype,"text/x-c",strlen("text/x-c"))==0){
			printf("text/x-c\n");
			return 0;
		}*/
		magic_close(magic_cookie);
        return -1;
}

// Translate an rofs path into it's underlying filesystem path
static char* translate_path(const char* path)
{
	printf("translate_path started\n");
	//printf("path: %s\n",path);
    char *rPath= malloc(sizeof(char)*(strlen(path)+strlen(rw_path)+1));

    strcpy(rPath,rw_path);
    if (rPath[strlen(rPath)-1]=='/') {
        rPath[strlen(rPath)-1]='\0';
    }
    strcat(rPath,path);
	printf("translate_path finished\n");
	
    return rPath;
}


static int rofs_getattr(const char *path, struct stat *st_data)
{
	printf("rofs_getattr started and path: %s\n",path);
    int res;
    //struct stat s;
    char *upath=translate_path(path);
    printf("rofs_getattr upath %s\n",upath);
    char *new_path=find_original_path(upath);
    printf("find_original_path: %s\n", new_path);
    
    if(new_path==NULL){
		printf("new path null: %s\n", upath);
		res = lstat(upath, st_data);
		st_data->st_size = 1000;
		
	}else{
		printf("new path sent %s\n",new_path);
		res = lstat(new_path, st_data);
		int *len = (int *)malloc(sizeof(int));
		char *buffer=ansi2png(new_path,len);
		st_data->st_size = *len;
		free(len);
		free(buffer);
	}
	char perm[25] = {0};
	sprintf (perm, "%8o", st_data->st_mode);
	//strmode(st_data->st_mode, perm);
	mode_t mode = strtol(perm, NULL, 8);
	printf("yetki st_data.st_mode :%o\n", mode);
	if(st_data->st_mode & S_IFDIR){
		st_data->st_mode = S_IFDIR | 0444;
	}else if(st_data->st_mode & S_IFREG ){
		st_data->st_mode = S_IFREG  | 0444;
	}
	
	if(new_path!=NULL)
		free(new_path);
	free(upath);
    
    printf("rofs_getattr finished\n");
    if(res == -1) {
        return -errno;
    }
    return 0;
}

static int rofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi)
{
	printf("rofs_readdir started\n");
    DIR *dp;
    struct dirent *de;
    int res;

    (void) offset;
    (void) fi;

    char *upath=translate_path(path);
    dp = opendir(upath);
	
    if(dp == NULL) {
        res = -errno;
        free(upath);
        return res;
    }
    
	int file_type = -1;
    while((de = readdir(dp)) != NULL) {
		
		if(de->d_type == DT_REG){
			//printf("strlen(de->d_name): %d\n(strlen(upath)): %d\n",strlen(de->d_name),strlen(upath));
			char *temp_filename = (char *)malloc(sizeof(char)*(strlen(de->d_name) + strlen(upath) + 2));
			
			if(strcmp(upath, "src/")!=0){
				strcpy(temp_filename, upath);
				temp_filename[strlen(upath)]='/';
				temp_filename[strlen(upath) + 1]='\0';
			}else{
				strcpy(temp_filename, upath);
			}
			
			//printf("once filename regular: %s\n filename: %s\n",temp_filename, de->d_name);
			strcat(temp_filename, de->d_name);
			temp_filename[strlen(de->d_name)+strlen(upath) + 1] ='\0';
			//printf("sonra filename regular: %s\nfilename: %s\n",temp_filename,de->d_name);
			//magic_checker(temp_filename);
			printf("filename regular files: %s\n",temp_filename);
			file_type = magic_checker(temp_filename);
			printf("file type: %d\n",file_type);
			free(temp_filename);

		}
		
		if(file_type==0){
			
			file_type = -1;
			
			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = de->d_ino;
			st.st_mode = de->d_type << 12;

			char *f_name = (char *)malloc(sizeof(char)*(strlen(de->d_name)+5));
			strcpy(f_name, de->d_name);
			int i=strlen(de->d_name);
			int flag=0;
			for(;i>=0;i--){
				if(f_name[i]=='.'){
					flag=1;
					f_name[++i]='p';
					f_name[++i]='n';
					f_name[++i]='g';
					f_name[++i]='\0';
					break;
				}
			}
			if(flag==0){
				f_name[strlen(de->d_name)]='.';
				f_name[strlen(de->d_name)+1]='p';
				f_name[strlen(de->d_name)+2]='n';
				f_name[strlen(de->d_name)+3]='g';
				f_name[strlen(de->d_name)+4]='\0';
			}
			printf("f_name: %s\n",f_name);
			if (filler(buf, f_name, &st, 0)){
				free(f_name);
				break;
			}
			free(f_name);
		}else if(de->d_type==DT_DIR ){
			file_type = -1;
			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = de->d_ino;
			st.st_mode = de->d_type << 12;
			
			if (filler(buf, de->d_name, &st, 0)){
				break;
			}
		}
    }
    

    closedir(dp);
    free(upath);
    return 0;
}

static int rofs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    (void)path;
    (void)mode;
    (void)rdev;
    return -EROFS;
}

static int rofs_mkdir(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return -EROFS;
}

static int rofs_unlink(const char *path)
{
    (void)path;
    return -EROFS;
}

static int rofs_rmdir(const char *path)
{
    (void)path;
    return -EROFS;
}

static int rofs_symlink(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int rofs_rename(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int rofs_link(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int rofs_chmod(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return -EROFS;

}

static int rofs_chown(const char *path, uid_t uid, gid_t gid)
{
    (void)path;
    (void)uid;
    (void)gid;
    return -EROFS;
}

static int rofs_truncate(const char *path, off_t size)
{
    (void)path;
    (void)size;
    return -EROFS;
}

static int rofs_utime(const char *path, struct utimbuf *buf)
{
    (void)path;
    (void)buf;
    return -EROFS;
}
static int rofs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)finfo;
    return -EROFS;
}


static int rofs_open(const char *path, struct fuse_file_info *finfo)
{	
	printf("rofs_open started and rofs_open path: %s\n",path);
	
    int res;

    /* We allow opens, unless they're tring to write, sneaky
     * people.
     */
    
     
    int flags = finfo->flags;

    if ((flags & O_WRONLY) || (flags & O_RDWR) || (flags & O_CREAT) || (flags & O_EXCL) || (flags & O_TRUNC) || (flags & O_APPEND)) {
        return -EROFS;
    }
	char *upath=translate_path(path);
    printf("rofs_open upath %s\n",upath);
    char *new_path=find_original_path(upath);
    printf("find_original_path: %s\n", new_path);
    
    if(new_path==NULL){
		printf("new path null: %s\n", upath);
		res = open(upath, flags);
	}else{
		printf("new path sent %s\n",new_path);
		res = open(new_path, flags);
	}
	if(new_path!=NULL)
		free(new_path);
	free(upath);
    
	printf("rofs_open finished with res = %d\n", res);
    if(res == -1) {
        return -errno;
    }
    close(res);
    return 0;
}

static int rofs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
	printf("rofs_read started\n");
    int fd;
    int res;
    (void)finfo;
    

    char *upath=translate_path(path);
    printf("rofs_open upath %s\n",upath);
    char *new_path=find_original_path(upath);
    printf("find_original_path: %s\n", new_path);
    
    char *actual_path=NULL;
    if(new_path==NULL){
		printf("new path null: %s\n", upath);
		fd = open(upath, O_RDONLY);
		actual_path = upath;
	}else{
		printf("new path sent %s\n",new_path);
		fd = open(new_path, O_RDONLY);
		actual_path = new_path;
	}
 
    if(fd == -1) {
        res = -errno;
        free(upath);
        return res;
    }
    
    int *len = (int *)malloc(sizeof(int));
    char *buffer=ansi2png(actual_path,len);
    if ((int)offset < *len) {
        if ((int)offset + (int)size > *len){
            res = *len - offset;
		}
        else{
			res=size;
		}    
        memcpy(buf, buffer + offset, res);
    } else{
        res = 0;
	}
	if(new_path!=NULL)
		free(new_path);
	free(len);
	free(upath);
	free(buffer);
	
	printf("rofs_read finished with res = %d\n", res);
	
    if(res == -1) {
        res = -errno;
    }
    close(fd);
    return res;
}


static int rofs_release(const char *path, struct fuse_file_info *finfo)
{
	printf("rofs_release started\n");
    (void) path;
    (void) finfo;
    return 0;
    printf("rofs_release finished\n");
}
/*
 * Set the value of an extended attribute
 */
static int rofs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    return -EROFS;
}
/*
 * Remove an extended attribute.
 */
static int rofs_removexattr(const char *path, const char *name)
{
    (void)path;
    (void)name;
    return -EROFS;

}
struct fuse_operations rofs_oper = {
    .getattr     = rofs_getattr,
    .readdir     = rofs_readdir,
    .mknod       = rofs_mknod,
    .mkdir       = rofs_mkdir,
    .symlink     = rofs_symlink,
    .unlink      = rofs_unlink,
    .rmdir       = rofs_rmdir,
    .rename      = rofs_rename,
    .link        = rofs_link,
    .chmod       = rofs_chmod,
    .chown       = rofs_chown,
    .truncate    = rofs_truncate,
    .utime       = rofs_utime,
    .open        = rofs_open,
    .read        = rofs_read,
    .write       = rofs_write,
    .release     = rofs_release,

    /* Extended attributes support for userland interaction */
    .setxattr    = rofs_setxattr,
    .removexattr = rofs_removexattr

};
enum {
    KEY_HELP,
    KEY_VERSION,
};

static void usage(const char* progname)
{
    fprintf(stdout,
            "usage: %s readwritepath mountpoint [options]\n"
            "\n"
            "   Mounts readwritepath as a read-only mount at mountpoint\n"
            "\n"
            "general options:\n"
            "   -o opt,[opt...]     mount options\n"
            "   -h  --help          print help\n"
            "   -V  --version       print version\n"
            "\n", progname);
}

static int rofs_parse_opt(void *data, const char *arg, int key,
                          struct fuse_args *outargs)
{
    (void) data;

    switch (key)
    {
    case FUSE_OPT_KEY_NONOPT:
        if (rw_path == 0)
        {
            rw_path = strdup(arg);
            return 0;
        }
        else
        {
            return 1;
        }
    case FUSE_OPT_KEY_OPT:
        return 1;
    case KEY_HELP:
        usage(outargs->argv[0]);
        exit(0);
    case KEY_VERSION:
        fprintf(stdout, "ROFS version %s\n", rofsVersion);
        exit(0);
    default:
        fprintf(stderr, "see `%s -h' for usage\n", outargs->argv[0]);
        exit(1);
    }
    return 1;
}

static struct fuse_opt rofs_opts[] = {
    FUSE_OPT_KEY("-h",          KEY_HELP),
    FUSE_OPT_KEY("--help",      KEY_HELP),
    FUSE_OPT_KEY("-V",          KEY_VERSION),
    FUSE_OPT_KEY("--version",   KEY_VERSION),
    FUSE_OPT_END
};

int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int res;

    res = fuse_opt_parse(&args, &rw_path, rofs_opts, rofs_parse_opt);
    if (res != 0)
    {
        fprintf(stderr, "Invalid arguments\n");
        fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
        exit(1);
    }
    if (rw_path == 0)
    {
        fprintf(stderr, "Missing readwritepath\n");
        fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
        exit(1);
    }
	/*
	char *upath=translate_path("/deneme/hello.png");
	printf("upath_path: %s\n", upath);
	char *new_path=find_original_path(upath);
	printf("find_: %s\n", new_path);
    free(new_path);
    free(upath);
    return 0;*/
	//printf("rw_path: %s\n",rw_path);
    fuse_main(args.argc, args.argv, &rofs_oper, NULL);
    
	//ansi2png("ak67-get-wet.ans");
	
    return 0;
}
