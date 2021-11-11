#ifndef UEYE_WSTP_H
#define UEYE_WSTP_H

#include <string>
#include <stdio.h>
#include <wstp.h>
#include <cstring>
#include <array>
#include <string>
#include <functional>
#include <memory>

#include "util.h"

enum LinkState { VALID, ERROR, CLOSED, INVALID, DEFERRED, NONE };
enum State { WAITING_ACTIVATION, WAITING_PACKET, WAITING_PACKET_YIELD, ACTIVE_CONNECTION, NO_ACTIVE_CONNECTION};


/* expose these types */
typedef WSLINK WSLINK;
typedef WSYieldParameters WSYieldParameters;

typedef int(*yield_fn_t)(WSLINK,WSYieldParameters);
typedef std::function<int(State, int)> yield_callback_t;
typedef std::function<std::shared_ptr<Image>()> hook_get_t;
typedef std::function<void(std::shared_ptr<Image>)> hook_send_t;
typedef std::function<void(const std::string&, double)> hook_set_prop_t;
typedef std::function<void(int, int, int, int)> hook_imgcrop_t;

void rgb_to_rgb3(int *dst, const int *src, const size_t N, const size_t channels = 3);
void rgb3_to_rgb(int *dst, const int *src, const size_t N, const size_t channels = 3);
void bw_to_rgb(int *dst, const int *src, size_t N);


class MathematicaL {
    public:
        MathematicaL(const std::string &link_prefix, 
                const hook_get_t &gfn = nullptr,
                const hook_send_t &sfn = nullptr,
                const hook_set_prop_t &tfn = nullptr,
                const yield_callback_t &yfn = nullptr);
        ~MathematicaL();


        int  initialize_connection(int new_name = 1);
        void deinitialize_close();
        std::string get_linkname();
        int process_packet();
        int clear_error(LinkState);
        int activate_link();
        LinkState check_error();

        void register_hook_get(const hook_get_t &fn);
        void register_hook_send(const hook_send_t &fn);
        void register_hook_set_prop(const hook_set_prop_t &fn);
        void register_yield_callback(const yield_callback_t &fn);
        void register_hook_imgcrop(const hook_imgcrop_t &fn);
        void set_verbosity(int level);

        State get_state() const;
        WSENV mep;
        WSLINK mlp;

    private:
        int command_exposure();
        int command_get();
        int command_send();
        int command_imgcrop();
        int register_WSyield_fn(const yield_fn_t &yfn);
        int fprintf_l(FILE *f, const char *fmt, ...);

        const std::string mlink_prefix;
        std::string mlink_name;
        int mverbose = 1;
        unsigned int mcount = 0;
        hook_get_t hook_get;
        hook_send_t hook_send;
        hook_set_prop_t hook_set_prop;
        hook_imgcrop_t hook_imgcrop;
        yield_callback_t yield_callback;

        State state; 

    friend int default_yield_fn(WSLINK lp, WSYieldParameters p);

    static const char *str_WSType(int type)
    {
        const char * ret = "unknown";
        switch (type)
        {
            case WSTKERR:
                ret = "WSTKERR";
                break;
            case WSTKINT:
                ret = "WSKINT";
                break;
            case WSTKFUNC:
                ret = "WSTKFUNC";
                break;
            case WSTKREAL:
                ret = "WSTKREAL";
                break;
            case WSTKSTR:
                ret = "WSTKSTR";
                break;
            case WSTKSYM:
                ret = "WSTKSYM";
                break;
            case WSTKOLDINT:
                ret = "WSTKOLDINT";
                break;
            case WSTKOLDREAL:
                ret = "WSTKOLDREAL";
                break;
            case WSTKOLDSTR:
                ret = "WSTKOLDSTR";
                break;
            case WSTKOLDSYM:
                ret = "WSTKOLDSYM";
                break;
            case WSTKOPTSTR:
                ret = "WSTKOPTSTR";
                break;
            case WSTKOPTSYM:
                ret = "WSTKOPTSYM";
                break;
        }

        return ret;
    }
};


#endif
