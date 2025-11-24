/* T3 includes */
#include "vmmaincn.h"
#include "vmhost.h"
#include "resload.h"

/* ------------------------------------------------------------------------ */
/*
 *   Client services interface.  Base our implementation on the standard
 *   console-based implementation, since we use a regular system console to
 *   implement the main HTML output stream.  
 */
class MyClientIfc : public CVmMainClientConsole{
public:
	/* set "plain" mode - do nothing, since we only run in GUI mode */
	void set_plain_mode() { }
	
	void display_error(struct vm_globals *, const struct CVmException *exc,
                   const char *msg, int /*add_blank_line*/)
	{
		/* display the message on the console */
		os_printz(msg);
	}
};

/* ------------------------------------------------------------------------ */
/*
 *   Host application interface.  This provides a bridge between the T3 VM
 *   host interface (class CVmHostIfc) and the TADS 2 application context
 *   (struct appctxdef) mechanism.  
 */
class MyHostIfc: public CVmHostIfc
{
public:
    MyHostIfc(appctxdef *appctx, const char *exe_dir, const char *exe_file)
    {
        /* remember the executable file */
        exe_file_ = lib_copy_str(exe_file);

        /* remember the TADS 2 host application context */
        appctx_ = appctx;

        /* set up the system resource loader in the executable directory */
        sys_res_loader_ = new CResLoader(exe_dir);

        /* set the name of the executable file */
        sys_res_loader_->set_exe_filename(exe_file);
        
        /* by default, allow read/write in current directory only */
        io_safety_read_ = VM_IO_SAFETY_READWRITE_CUR;
        io_safety_write_ = VM_IO_SAFETY_READWRITE_CUR;
    }

    ~MyHostIfc()
    {
        /* delete the system resource loader */
        delete sys_res_loader_;

        /* delete the saved executable file name */
        lib_free_str(exe_file_);
    }

    /* get the I/O safety level */
    int get_io_safety_read()
    {
        int read, write;
        get_io_safety(&read, &write);
        return read;
    }

    int get_io_safety_write()
    {
        int read, write;
        get_io_safety(&read, &write);
        return write;
    }
    
    /* get the I/O safety level */
    void get_io_safety(int *read, int *write)
    {
        if (appctx_ != 0 && appctx_->get_io_safety_level != 0)
        {
            /* ask the app context to handle it */
            (*appctx_->get_io_safety_level)
                (appctx_->io_safety_level_ctx, read, write);
        }
        else
        {
            /* the app context doesn't care - use our own level memory */
            *read = io_safety_read_;
            *write = io_safety_write_;
        }
    }

    /* set the I/O safety level */
    void set_io_safety(int read, int write)
    {
        if (appctx_ != 0 && appctx_->set_io_safety_level != 0)
        {
            /* let the app context handle it */
            (*appctx_->set_io_safety_level)(appctx_->io_safety_level_ctx,
                                            read, write);
        }
        else
        {
            /* the app doesn't care - set our own level memory */
            io_safety_read_ = read;
            io_safety_write_ = write;
        }
    }

    /* get the network safety level */
    void get_net_safety(int *client_level, int *server_level)
    {
        if (appctx_ != 0 && appctx_->get_net_safety_level != 0)
        {
            /* ask the app context to handle it */
            return (*appctx_->get_net_safety_level)
                (appctx_->net_safety_level_ctx, client_level, server_level);
        }
        else
        {
            /* the app context doesn't care - use our own level memory */
            *client_level = net_client_safety_;
            *server_level = net_server_safety_;
        }
    }

    /* get the network safety level */
    void set_net_safety(int client_level, int server_level)
    {
        if (appctx_ != 0 && appctx_->set_net_safety_level != 0)
        {
            /* let the app context handle it */
            (*appctx_->set_net_safety_level)(appctx_->net_safety_level_ctx,
                                             client_level, server_level);
        }
        else
        {
            /* the app doesn't care - set our own level memory */
            net_client_safety_ = client_level;
            net_server_safety_ = server_level;
        }
    }

    /* get the resource loader */
    CResLoader *get_sys_res_loader() { return sys_res_loader_; }

    /* set the game name */
    void set_image_name(const char *fname)
    {
        /* pass it through the app context if possible */
        if (appctx_ != 0 && appctx_->set_game_name != 0)
            (*appctx_->set_game_name)(appctx_->set_game_name_ctx, fname);
    }

    /* set the resource root directory */
    void set_res_dir(const char *fname)
    {
        /* pass it through the app context if possible */
        if (appctx_ != 0 && appctx_->set_res_dir != 0)
            (*appctx_->set_res_dir)(appctx_->set_res_dir_ctx, fname);
    }

    /* add a resource file */
    int add_resfile(const char *fname)
    {
        /* pass it through the app context if possible */
        if (appctx_ != 0 && appctx_->add_resfile != 0)
            return (*appctx_->add_resfile)(appctx_->add_resfile_ctx, fname);
        else
            return 0;
    }

    /* we suport add_resfile() if the application context does */
    virtual int can_add_resfiles()
    {
        /* 
         *   if the add_resfile function is defined in the application
         *   context, we support adding resource files 
         */
        return (appctx_ != 0 && appctx_->add_resfile != 0);
    }

    /* add a resource */
    void add_resource(unsigned long ofs, unsigned long siz,
                      const char *res_name, size_t res_name_len, int fileno)
    {
        /* pass it through the app context if possible */
        if (appctx_ != 0 && appctx_->add_resource != 0)
            (*appctx_->add_resource)(appctx_->add_resource_ctx, ofs, siz,
                                     res_name, res_name_len, fileno);
    }

    /* add a resource link */
    void add_resource(const char *fname, size_t fname_len,
                      const char *res_name, size_t res_name_len)
    {
        /* pass it through the app context if possible */
        if (appctx_ != 0 && appctx_->add_resource_link != 0)
            (*appctx_->add_resource_link)(appctx_->add_resource_link_ctx,
                                          fname, fname_len,
                                          res_name, res_name_len);
    }

    /* get the external resource path */
    const char *get_res_path()
    {
        /* get the path from the app context if possible */
        return (appctx_ != 0 ? appctx_->ext_res_path : 0);
    }

    /* determine if a resource exists */
    int resfile_exists(const char *res_name, size_t res_name_len)
    {
        /* 
         *   let the application context handle it if possible; if not,
         *   just return false, since we can't otherwise provide resource
         *   operations 
         */
        if (appctx_ != 0 && appctx_->resfile_exists != 0)
            return (*appctx_->resfile_exists)(appctx_->resfile_exists_ctx,
                                              res_name, res_name_len);
        else
            return FALSE;
    }

    /* find a resource */
    osfildef *find_resource(const char *res_name, size_t res_name_len,
                            unsigned long *res_size)
    {
        /* 
         *   let the application context handle it; if we don't have an
         *   application context, we don't provide resource operation, so
         *   simply return failure 
         */
        if (appctx_ != 0 && appctx_->find_resource != 0)
            return (*appctx_->find_resource)(appctx_->find_resource_ctx,
                                             res_name, res_name_len,
                                             res_size);
        else
            return 0;
    }

    /* get the image file name */
    vmhost_gin_t get_image_name(char *buf, size_t buflen)
    {
        /* 
         *   let the application context handle it if possible; otherwise,
         *   return false, since we can't otherwise ask for an image name 
         */
        if (appctx_ != 0 && appctx_->get_game_name != 0)
        {
            int ret;
            
            /* ask the host system to get a name */
            ret = (*appctx_->get_game_name)(appctx_->get_game_name_ctx,
                                            buf, buflen);

            /* 
             *   if that failed, the user must have chosen to cancel;
             *   otherwise, we were successful
             */
            return (ret ? VMHOST_GIN_SUCCESS : VMHOST_GIN_CANCEL);
        }
        else
        {
            /* we can't ask for a name */
            return VMHOST_GIN_IGNORED;
        }
    }

    /* get a special file system path */
    virtual void get_special_file_path(char *buf, size_t buflen, int id)
        { return os_get_special_path(buf, buflen, exe_file_, id); }

private:
    /* TADS 2 host application context */
    appctxdef *appctx_;

    /* system resource loader (character map files, etc) */
    CResLoader *sys_res_loader_;

    /* 
     *   our internal I/O safety level memory (in case the host app
     *   context doesn't support I/O safety level operations) 
     */
    int io_safety_read_;
    int io_safety_write_;

    /* internal network safety level settings */
    int net_client_safety_;
    int net_server_safety_;

    /* the main executable filename */
    char *exe_file_;
};

int t3main(int argc, char **argv, struct appctxdef *app_ctx, char *config_file){
	MyClientIfc clientifc;
	CVmHostIfc* hostifc;
	/* run the program and return the result code */
	int stat = vm_run_image_main(&clientifc, "htmlt3",
							 argc, argv, true, false, hostifc);
	return stat;
}