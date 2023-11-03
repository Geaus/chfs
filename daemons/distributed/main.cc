/**
 * Credits: the comments of each function implemented is copied from
 * `bbfs`: https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/html/index.html
 *
 * Basically, this function wrap the underlying CHFS operations with FUSE.
 * As a result, we can support a POSIX-like API.
 */

#define FUSE_USE_VERSION 26
#include <fuse/fuse_lowlevel.h>

#include <iostream>
#include <string>
#include <unistd.h>

#include "./consts.h"
#include "distributed/client.h"
#include "filesystem/directory_op.h"

#include "argparse/argparse.hpp"

// This header must be include at the bottom
// Since we need to redefine some MACROS in CHFS
namespace chfs {

auto getattr_helper(InodeType type, const FileAttr &attr) -> struct stat {
  struct stat st;
  st.st_nlink = 1;
  st.st_atime = attr.atime;
  st.st_mtime = attr.mtime;
  st.st_ctime = attr.ctime;
  st.st_size = attr.size;

  switch (type) {
  case InodeType::FILE:
    st.st_mode = S_IFREG | 0666;
    break;
  case InodeType::Directory:
    st.st_mode = S_IFDIR | 0777;
    st.st_nlink += 1;
    break;
  default:
    // links
    UNIMPLEMENTED();
  }
  return st;
}

static void
chfs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
  ChfsClient *fs = reinterpret_cast<ChfsClient *>(fuse_req_userdata(req));

  auto attr = fs->get_type_attr(ino);
  if (attr.is_err()) {
    // entry not exist
    fuse_reply_err(req, ENOENT);
  } else {
    auto type_attr = attr.unwrap();
    auto attr = std::get<1>(type_attr);
    auto st = getattr_helper(std::get<0>(type_attr), attr);

    fuse_reply_attr(req, &st, 0);
  }
}

struct DirectoryBuf {
  char *p;
  size_t size;

  DirectoryBuf() : p(nullptr), size(0) {}

  auto add(fuse_req_t req, const char *name, fuse_ino_t ino) -> void {
    // it will fail, anyway
    auto sz = fuse_add_direntry(req, nullptr, 0, name, nullptr, size);
    auto oldsize = size;
    size += sz;
    p = (char *)realloc(p, size);
    memset(p + oldsize, 0, size - oldsize);

    struct stat stbuf = {};
    stbuf.st_ino = ino;

    fuse_add_direntry(req, p + oldsize, size - oldsize, name, &stbuf, size);
  }

  auto reply_buf_limited(fuse_req_t req, off_t off, size_t maxsize) -> int {
    if ((size_t)off < this->size)
      return fuse_reply_buf(req, this->p + off,
                            std::min(this->size - off, maxsize));
    else
      return fuse_reply_buf(req, nullptr, 0);
  }

  ~DirectoryBuf() { free(p); }
};

void chfs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                  struct fuse_file_info *fi) {
  ChfsClient *fs = reinterpret_cast<ChfsClient *>(fuse_req_userdata(req));

  auto readdir_res = fs->readdir(ino);
  if (readdir_res.is_err()) {
    fuse_reply_err(req, ENOTDIR);
    return;
  }

  auto list = readdir_res.unwrap();

  DirectoryBuf buf;
  for (auto &[name, id] : list) {
    buf.add(req, name.c_str(), id);
  }
  buf.reply_buf_limited(req, off, size);
}

void chfs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
               struct fuse_file_info *fi) {

  ChfsClient *fs = reinterpret_cast<ChfsClient *>(fuse_req_userdata(req));

  std::cerr << "Read file from inode " << ino << " at off " << off << " with " << size << std::endl;
  auto attr_res = fs->get_type_attr(ino);
  if (attr_res.is_err()) {
    fuse_reply_err(req, -1); // fatal error
    return;
  }

  auto type_attr = attr_res.unwrap();
  auto attr = std::get<1>(type_attr);
  auto st = getattr_helper(std::get<0>(type_attr), attr);

  if (off >= st.st_size) {
    fuse_reply_buf(req, nullptr, 0);
    return;
  }


  auto read_res = fs->read_file(ino, off, size);
  if (read_res.is_err()) {
    std::cerr << "Bad read requests" << std::endl;
    fuse_reply_err(req, -1);
    return;
  }

  auto res_data = read_res.unwrap();
  fuse_reply_buf(req, reinterpret_cast<const char *>(res_data.data()),
                 size);
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to bb_readlink()
void chfs_readlink(fuse_req_t req, fuse_ino_t ino) { UNIMPLEMENTED(); }

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
void chfs_mknod(fuse_req_t req, fuse_ino_t parent, const char *name,
                mode_t mode, dev_t rdev) {

  ChfsClient *fs = reinterpret_cast<ChfsClient *>(fuse_req_userdata(req));
  struct fuse_entry_param e;

  // In chfs, timeouts are always set to 0.0, and generations are always set to
  // 0
  e.attr_timeout = 0.0;
  e.entry_timeout = 0.0;
  e.generation = 0;

  auto res = fs->mknode(ChfsClient::FileType::REGULAR, parent, name);

  if (res.is_ok()) {
    e.ino = res.unwrap();
    auto attr_res = fs->get_type_attr(e.ino);
    if (attr_res.is_err()) {
      fuse_reply_err(req, -1);
      return;
    }

    auto type_attr = attr_res.unwrap();
    auto attr = std::get<1>(type_attr);
    auto st = getattr_helper(std::get<0>(type_attr), attr);
    memcpy(&e.attr, &st, sizeof(struct stat));
    fuse_reply_entry(req, &e);
    return;
  } else {
    auto err = res.unwrap_error();
    if (err == ErrorType::AlreadyExist) {
      fuse_reply_err(req, EEXIST);
      return;
    }
    fuse_reply_err(req, ENOENT);
    return;
  }
}

/** Create a directory */
void chfs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
                mode_t mode) {
  struct fuse_entry_param e;

  // In chfs, timeouts are always set to 0.0, and generations are always set to
  // 0
  e.attr_timeout = 0.0;
  e.entry_timeout = 0.0;
  e.generation = 0;

  /**
   * { Your code here }
   */
  // UNIMPLEMENTED();

  ChfsClient *fs = reinterpret_cast<ChfsClient *>(fuse_req_userdata(req));

  // FIXME: a simple impl will ignore the mode
  auto res = fs->mknode(ChfsClient::FileType::DIRECTORY, parent, name);
  if (res.is_err()) {
    switch (res.unwrap_error()) {
    case ErrorType::AlreadyExist:
      fuse_reply_err(req, EEXIST);
      break;
    default:
      fuse_reply_err(req, ENOSYS);
    }
    return;
  }

  e.ino = res.unwrap();

  auto attr_res = fs->get_type_attr(e.ino);
  if (attr_res.is_err()) {
    fuse_reply_err(req, -1);
    return;
  }

  auto type_attr = attr_res.unwrap();
  auto attr = std::get<1>(type_attr);
  auto st = getattr_helper(std::get<0>(type_attr), attr);
  memcpy(&e.attr, &st, sizeof(struct stat));
  fuse_reply_entry(req, &e);
}

/** Remove a file */
void chfs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name) {
  ChfsClient *fs = reinterpret_cast<ChfsClient *>(fuse_req_userdata(req));

  auto res = fs->unlink(parent, std::string(name));
  if (res.is_err()) {
    switch (res.unwrap_error()) {
    case ErrorType::NotExist:
      fuse_reply_err(req, ENOENT);
      break;
    case ErrorType::NotEmpty:
      fuse_reply_err(req, ENOTEMPTY);
      break;
    default:
      fuse_reply_err(req, ENOSYS);
    }
    return;
  } else {
    fuse_reply_err(req, 0);
  }
}

/** Remove a directory */
void chfs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name) {
  UNIMPLEMENTED();
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
void chfs_symlink(fuse_req_t req, const char *link, fuse_ino_t parent,
                  const char *name) {
  UNIMPLEMENTED();
}

//
// Set the attributes of a file. Often used as part of overwriting
// a file, to set the file length to zero.
//
// to_set is a bitmap indicating which attributes to set. You only
// have to implement the FUSE_SET_ATTR_SIZE bit, which indicates
// that the size of the file should be changed. The new size is
// in attr->st_size. If the new size is bigger than the current
// file size, fill the new bytes with '\0'.
//
// On success, call fuse_reply_attr, passing the file's new
// attributes (from a call to getattr()).
//
void chfs_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set,
                  struct fuse_file_info *fi) {
  ChfsClient *fs = reinterpret_cast<ChfsClient *>(fuse_req_userdata(req));

  // do nothing
    auto attr_res = fs->get_type_attr(ino);
    if (attr_res.is_err()) {
      fuse_reply_err(req, -1); // fatal error
      return;
    }
    auto type_attr = attr_res.unwrap();
    auto file_attr = std::get<1>(type_attr);
    auto st = getattr_helper(std::get<0>(type_attr), file_attr);
    fuse_reply_attr(req, &st, 0);
    return;
}

/** Rename a file */
// both path and newpath are fs-relative
void chfs_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
                 fuse_ino_t newparent, const char *newname) {
  UNIMPLEMENTED();
}

/** Create a hard link to a file */
void chfs_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent,
               const char *newname) {
  UNIMPLEMENTED();
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
void chfs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
  // we adopt a simplified implementation
  fuse_reply_open(req, fi);
}

/** Write data to an open file
 *
 */
void chfs_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size,
                off_t off, struct fuse_file_info *fi) {
  // std::cerr << "[chfs write] file " << ino << " with size {" << size << "}"
  //          << " and off: {" << off << "}.";

  ChfsClient *fs = reinterpret_cast<ChfsClient *>(fuse_req_userdata(req));
  auto data = std::vector<u8>(buf, buf + size);
  auto write_res = fs->write_file(ino, off, data);
  if (write_res.is_err()) {
    fuse_reply_err(req, ENOSYS);
  } else {
    fuse_reply_write(req, size);
  }
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
void chfs_statfs(fuse_req_t req, fuse_ino_t ino) { UNIMPLEMENTED(); }

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
// this is a no-op in BBFS.  It just logs the call and returns success
void chfs_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
  UNIMPLEMENTED();
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
void chfs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
  UNIMPLEMENTED();
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
void chfs_fsync(fuse_req_t req, fuse_ino_t ino, int datasync,
                struct fuse_file_info *fi) {
  UNIMPLEMENTED();
}

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
void chfs_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
  UNIMPLEMENTED();
}

/** Release directory
 *
 * Introduced in version 2.3
 */
void chfs_releasedir(fuse_req_t req, fuse_ino_t ino,
                     struct fuse_file_info *fi) {
  UNIMPLEMENTED();
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
void chfs_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync,
                   struct fuse_file_info *fi) {
  UNIMPLEMENTED();
}

static struct fuse_lowlevel_ops fuseserver_oper;

void usage() {
  std::cerr << "Usage: chfs [FUSE and mount options] mountPoint" << std::endl;
  abort();
}

auto pre_process() -> void {
  if ((getuid() == 0) || (geteuid() == 0)) {
    std::cerr << "Running CHFS as root opens unnacceptable security holes"
              << std::endl;
    usage();
    abort();
    exit(-1);
  }
}

auto bootstrap_fuse(int argc, char **argv) -> int {
  const char *fuse_argv[20];
  int fuse_argc = 0;
  fuse_argv[fuse_argc++] = argv[0];
  auto mountpoint = argv[1];

  // prepare the fuse parameters
#ifdef __APPLE__
  fuse_argv[fuse_argc++] = "-o";
  fuse_argv[fuse_argc++] = "nolocalcaches"; // no dir entry caching
  fuse_argv[fuse_argc++] = "-o";
  fuse_argv[fuse_argc++] = "daemon_timeout=86400";
#endif

  fuse_argv[fuse_argc++] = mountpoint;
  fuse_argv[fuse_argc++] = "-d";
  fuse_args args = FUSE_ARGS_INIT(fuse_argc, (char **)fuse_argv);
  int foreground;

  int res =
      fuse_parse_cmdline(&args, &mountpoint, 0 /*multithreaded*/, &foreground);

  if (res == -1) {
    std::cerr << "fuse_parse_cmdline failed\n";
    return 0;
  }

  auto ch = fuse_mount(argv[1], &args);
  if (ch == nullptr) {
    std::cerr << "Fuse mount fails. " << std::endl;
    exit(1);
  }

  // 2. prepare the filesystem client
  auto fs = new ChfsClient();
  fs->reg_server(ChfsClient::ServerType::METADATA_SERVER, "127.0.0.1", kMetadataServerPort, true);
  for (auto i = 0; i < kDataServerNum; ++i)
    fs->reg_server(ChfsClient::ServerType::DATA_SERVER, "127.0.0.1", kDataServerPorts[i], true);

  auto se =
      fuse_lowlevel_new(&args, &fuseserver_oper, sizeof(fuseserver_oper), fs);
  if (se == 0) {
    std::cerr << "fuse_lowlevel_new failed\n";
    exit(1);
  }

  fuse_session_add_chan(se, ch);
  auto err = fuse_session_loop(se);

  fuse_session_destroy(se);
  fuse_unmount(argv[1], ch);

  delete fs;
  return err;
}

// We assume the lookup must be conducted on a directory inode
void chfs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
  ChfsClient *fs = reinterpret_cast<ChfsClient *>(fuse_req_userdata(req));
  struct fuse_entry_param e;

  // In chfs, timeouts are always set to 0.0, and generations are always set to
  // 0
  e.attr_timeout = 0.0;
  e.entry_timeout = 0.0;
  e.generation = 0;

  // lookup
  auto lookup_res = fs->lookup(parent, name);
  if (lookup_res.is_err()) {
    auto err = lookup_res.unwrap_error();
    if (err == ErrorType::NotExist) {
      fuse_reply_err(req, ENOENT);
      return;
    }
    fuse_reply_err(req, -1);
    return;
  }

  auto lookup = lookup_res.unwrap();
  if (lookup == 0) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  e.ino = lookup;
 
  auto attr_res = fs->get_type_attr(e.ino);
  if (attr_res.is_err()) {
    fuse_reply_err(req, -1);
    return;
  }

  auto type_attr = attr_res.unwrap();
  auto attr = std::get<1>(type_attr);
  auto st = getattr_helper(std::get<0>(type_attr), attr);
  memcpy(&e.attr, &st, sizeof(struct stat));

  fuse_reply_entry(req, &e);

  return;
}

} // namespace chfs

/**
 * Usage:
 *
 * ./bin/fs directory_to_mount xxx
 */
auto main(int argc, char **argv) -> int {
  using namespace chfs;
  pre_process();

  // Not all functions are needed for our tests
  fuseserver_oper.open = chfs_open;
  fuseserver_oper.getattr = chfs_getattr;
  fuseserver_oper.readdir = chfs_readdir;
  fuseserver_oper.read = chfs_read;
  fuseserver_oper.readlink = chfs_readlink;
  fuseserver_oper.mknod = chfs_mknod;
  fuseserver_oper.mkdir = chfs_mkdir;
  fuseserver_oper.unlink = chfs_unlink;
  fuseserver_oper.rmdir = chfs_rmdir;
  fuseserver_oper.symlink = chfs_symlink;
  fuseserver_oper.rename = chfs_rename;
  fuseserver_oper.link = chfs_link;
  fuseserver_oper.write = chfs_write;
  fuseserver_oper.setattr = chfs_setattr;
  fuseserver_oper.statfs = chfs_statfs;
  // fuseserver_oper.flush = chfs_flush;
  // fuseserver_oper.release = chfs_release;
  // fuseserver_oper.fsync = chfs_fsync;
  // fuseserver_oper.opendir = chfs_opendir;
  // fuseserver_oper.releasedir = chfs_releasedir;
  // fuseserver_oper.fsyncdir = chfs_fsyncdir;
  fuseserver_oper.lookup = chfs_lookup;

  std::cout << "Start to hook fuse" << std::endl;
  std::cout << "CHFS FUSE log started\n";

  auto ret = bootstrap_fuse(argc, argv);
  std::cout << "Fuse main return with [" << ret << "]. " << std::endl;

  if (ret != 0) {
    usage();
  }
  return ret;
}
