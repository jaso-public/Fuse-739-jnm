
#include <iostream>
#include <memory>
#include <string>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/xattr.h>

#define _XOPEN_SOURCE 700
#define ERRNO_NOOP -999

#include "fuse.h"

#include <grpcpp/grpcpp.h>
#include "helloworld.grpc.pb.h"


using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientReader;


using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;
using helloworld::CommonRequest;
using helloworld::CommonResponse;
using helloworld::Data;

extern int debug_flag;

class GreeterClient {
public:
    GreeterClient(std::shared_ptr<Channel> channel) : stub_(Greeter::NewStub(channel)) {}

    // Assembles the client's payload, sends it and presents the response back
    // from the server.
    std::string SayHello(const std::string& user) {
        // Data we are sending to the server.
        HelloRequest request;
        request.set_name(user);

        // Container for the data we expect from the server.
        HelloReply reply;

        // Context for the client. It could be used to convey extra information to
        // the server and/or tweak certain RPC behaviors.
        ClientContext context;

        // The actual RPC.
        Status status = stub_->SayHello(&context, request, &reply);

        // Act upon its status.
        if (status.ok()) {
            return reply.message();
        } else {
            std::cout << status.error_code() << ": " << status.error_message()
                      << std::endl;
            return "RPC failed";
        }
    }

    void print_debug(const std::string& cmd, const std::string& path, int ret) {
        if (ret) {
            if (debug_flag) {
                std::cout << cmd << " path: [" << path << "] errno:" << errno << " " << strerror(errno) << std::endl;
            }
        } else {
            if (debug_flag) {
                std::cout << cmd << " path: [" << path << "] OK" << std::endl;
            }
        }
    }

    int respond(const std::string& cmd, const std::string& path, Status status, int ret) {
        if (status.ok()) {
            if(ret) errno = -ret;
            print_debug(cmd, path, ret);
            return ret ? -1 : 0;
        } else {
            if (debug_flag) {
                std::cout << cmd << " path: [" << path << "] rpc not ok -- " << status.error_code() << ": " << status.error_message() << std::endl;
            }
            return -1;
        }
    }

    int rmdir(const std::string& path) {
        CommonRequest request;
        request.set_path1(path);

        CommonResponse response;
        ClientContext context;
        Status status = stub_->RPC_rmdir(&context, request, &response);
        return respond("rmdir", path, status, response.result());
    }

    int receive_file(const std::string& path, int dest_fd, size_t* size) {
        if(debug_flag) {
            std::cout << "starting receive_file " << path << " dest_fd:" << dest_fd << std::endl;
        }

        *size = 0;
        int err = 0;
        CommonRequest request;
        request.set_path1(path);

        ClientContext context;
        Data data;
        std::unique_ptr<ClientReader<Data>> reader(stub_->RPC_sendfile(&context, request));
        while (reader->Read(&data)) {
            if(data.result() != 0) {
                err = data.result();
                break;
            }

            int len = data.data().length();
            int ret = write(dest_fd, data.data().c_str(), len);
            if(ret!=len){
                err = errno;
                break;
            }
            *size += len;
        }

        Status status = reader->Finish();
        return respond("receive_file", path, status, err);
    }


    ~GreeterClient() {
        std::cout << "\n Destructor executed";
    }

private:
    std::unique_ptr<Greeter::Stub> stub_;
};

static GreeterClient* greeterPtr;


//
//public int read_file(const char *path, StreamObserver<Data> responseObserver) {) {
//    int fd = open(path, O_RDONLY);
//    if (fd == -1) {
//        Data ;
//        return;
//    }
//
//    while (1) {
//        int ret = read(fd, buf, sizeof(buf));
//        if (ret < 0) {
//            // error occured
//        } else if (ret == 0) {
//            // end of file reached
//        } else {
//            // we got data
//        }
//    }
//
//    close(fd);
//}

extern "C" int rpc_receive_file(const char *path, int fd, size_t* size) {
    return greeterPtr->receive_file(path, fd, size);
}



extern "C" int rpc_lstat(const char *path, struct stat *buf)
{
    memset(buf, 0, sizeof(struct stat));
    if (lstat(path, buf) == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_getattr(const char *path, struct stat *buf)
{
    memset(buf, 0, sizeof(struct stat));
    if (lstat(path, buf) == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_readlink(const char *path, char *buf, size_t bufsiz)
{
    int ret = readlink(path, buf, bufsiz);
    if (ret == -1) {
        return -errno;
    }
    buf[ret] = 0;

    return 0;
}

extern "C" int rpc_mknod(const char *path, mode_t mode, dev_t dev)
{
    int ret = mknod(path, mode, dev);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_mkdir(const char *path, mode_t mode)
{
    int ret = mkdir(path, mode);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_unlink(const char *path)
{
    int ret = unlink(path);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_rmdir(const char *path) {
    return greeterPtr->rmdir(path);
}

extern "C" int rpc_symlink(const char *target, const char *linkpath)
{
    int ret = symlink(target, linkpath);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_rename(const char *oldpath, const char *newpath)
{
    int ret = rename(oldpath, newpath);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_link(const char *oldpath, const char *newpath)
{
    int ret = link(oldpath, newpath);
    if (ret < 0) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_chmod(const char *path, mode_t mode)
{
    int ret = chmod(path, mode);
    if (ret < 0) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_chown(const char *path, uid_t owner, gid_t group)
{
    int ret = chown(path, owner, group);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_truncate(const char *path, off_t length)
{
    int ret = truncate(path, length);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_open(const char *path, struct fuse_file_info *fi)
{
    int ret = open(path, fi->flags);
    if (ret == -1) {
        return -errno;
    }
    fi->fh = ret;

    return 0;
}

extern "C" extern "C" int rpc_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    std::string result = greeterPtr->SayHello(path);
    const char* data = result.c_str();
    memcpy( buffer, data + offset, size );
    return strlen( data ) - offset;
}

extern "C" int rpc_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int fd;
    (void) fi;
    if(fi == NULL) {
        fd = open(path, O_WRONLY);
    } else {
        fd = fi->fh;
    }

    if (fd == -1) {
        return -errno;
    }

    int ret = pwrite(fd, buf, size, offset);
    if (ret == -1) {
        ret = -errno;
    }

    if(fi == NULL) {
        close(fd);
    }

    return ret;
}

extern "C" int rpc_statfs(const char *path, struct statvfs *buf)
{
    int ret = statvfs(path, buf);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_flush(const char *path, struct fuse_file_info *fi)
{
    int ret = close(dup(fi->fh));
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_release(const char *path, struct fuse_file_info *fi)
{
    int ret = close(fi->fh);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int ret;

    if (datasync) {
        ret = fdatasync(fi->fh);
        if (ret == -1) {
            return -errno;
        }
    } else {
        ret = fsync(fi->fh);
        if (ret == -1) {
            return -errno;
        }
    }

    return 0;
}

extern "C" int rpc_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    int ret = setxattr(path, name, value, size, flags);

    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int ret = getxattr(path, name, value, size);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_listxattr(const char *path, char *list, size_t size)
{
    int ret = listxattr(path, list, size);
    if (ret == -1) {
        return -errno;
    }

    return ret;
}

extern "C" int rpc_removexattr(const char *path, const char *name)
{
    int ret = removexattr(path, name);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dir = opendir(path);

    if (!dir) {
        return -errno;
    }
    fi->fh = (int64_t) dir;

    return 0;
}

extern "C" int rpc_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    DIR *dp = opendir(path);
    if (dp == NULL) {
        return -errno;
    }
    struct dirent *de;

    (void) offset;
    (void) fi;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0))
            break;
    }
    closedir(dp);

    return 0;
}

extern "C" int rpc_releasedir(const char *path, struct fuse_file_info *fi)
{
    DIR *dir = (DIR *) fi->fh;

    int ret = closedir(dir);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int ret;

    DIR *dir = opendir(path);
    if (!dir) {
        return -errno;
    }

    if (datasync) {
        ret = fdatasync(dirfd(dir));
        if (ret == -1) {
            return -errno;
        }
    } else {
        ret = fsync(dirfd(dir));
        if (ret == -1) {
            return -errno;
        }
    }
    closedir(dir);

    return 0;
}

extern "C" void *rpc_init(struct fuse_conn_info *conn) {
    fprintf(stdout, "initializing RPC\n");
    std::string target_str = "localhost:50051";
    greeterPtr = new GreeterClient(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
    std::string result = greeterPtr->SayHello("speedy");
    fprintf(stdout, "called server -- result:%s\n", result.c_str());
    return NULL;
}

extern "C" void rpc_destroy(void *private_data)
{
    delete greeterPtr;
}

extern "C" int rpc_access(const char *path, int mode)
{
    int ret = access(path, mode);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int ret = open(path, fi->flags, mode);
    if (ret == -1) {
        return -errno;
    }
    fi->fh = ret;

    return 0;
}

extern "C" int rpc_ftruncate(const char *path, off_t length, struct fuse_file_info *fi)
{
    int ret = truncate(path, length);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_fgetattr(const char *path, struct stat *buf, struct fuse_file_info *fi)
{
    int ret = fstat((int) fi->fh, buf);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *fl)
{
    int ret = fcntl((int) fi->fh, cmd, fl);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi, unsigned int flags, void *data)
{
    int ret = ioctl(fi->fh, cmd, arg);
    if (ret == -1) {
        return -errno;
    }

    return ret;
}

extern "C" int rpc_flock(const char *path, struct fuse_file_info *fi, int op)
{
    int ret = flock(((int) fi->fh), op);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

extern "C" int rpc_fallocate(const char *path, int mode, off_t offset, off_t len, struct fuse_file_info *fi)
{
    int fd;
    (void) fi;

    if (mode) {
	return -EOPNOTSUPP;
    }

    if(fi == NULL) {
	fd = open(path, O_WRONLY);
    } else {
	fd = fi->fh;
    }

    if (fd == -1) {
	return -errno;
    }

    int ret = fallocate((int) fi->fh, mode, offset, len);
    if (ret == -1) {
        return -errno;
    }

    if(fi == NULL) {
	close(fd);
    }

    return 0;
}

extern "C" int rpc_utimens(const char *path, const struct timespec ts[2])
{
    /* don't use utime/utimes since they follow symlinks */
    int ret = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
    if (ret == -1) {
        return -errno;
    }

    return 0;
}

