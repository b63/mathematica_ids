#include "MathematicaL.h"

#include <cstring>
#include <cstdarg>
#include <wstp.h>
#include <new>
#include <memory>

// global pointer to WSTP obj
// (can have only one WSTP instance alive per running program)
MathematicaL *G_WSTP_PTR;

MathematicaL::MathematicaL(const std::string &link_prefix,
        const hook_get_t &gfn,
        const hook_send_t &sfn,
        const hook_set_prop_t &tfn,
        const yield_callback_t &yfn)
    : mlink_prefix{link_prefix}, 
      hook_get{gfn},
      hook_send{sfn},
      yield_callback{yfn},
      hook_set_prop{tfn},
      mep {(WSENV)0},
      mlp {(WSLINK)0},
      state {NO_ACTIVE_CONNECTION}
{
    G_WSTP_PTR = this;
}

void MathematicaL::set_verbosity(int level)
{
    mverbose = level;
}

int default_yield_fn(WSLINK lp, WSYieldParameters p)
{
    // use global WSTP object to call method
    if (G_WSTP_PTR && G_WSTP_PTR->yield_callback)
        return G_WSTP_PTR->yield_callback(G_WSTP_PTR->get_state(), -1);

    // block by default
    return 0;
}


void print_2dint(const int *arr, size_t w, size_t h)
{
    for (int i = 0; i < w; ++i)
    {
        for (int j = 0; j < h; ++j)
        {
            printf("%3i", *(arr+w*i+j));
            if (j < h-1) printf(" ");
        }
        printf("\n");
    }
}

State MathematicaL::get_state() const {
    return state;
}

void MathematicaL::register_hook_get(const hook_get_t &fn)
{
    hook_get = fn;
}

void MathematicaL::register_hook_send(const hook_send_t &fn)
{
    hook_send = fn;
}

void MathematicaL::register_hook_set_prop(const hook_set_prop_t &fn)
{
    hook_set_prop = fn;
}

void MathematicaL::register_hook_imgcrop(const hook_imgcrop_t &fn)
{
    hook_imgcrop = fn;
}


void MathematicaL::register_yield_callback(const yield_callback_t &fn)
{
    yield_callback = fn;
}

int MathematicaL::fprintf_l(FILE *f, const char *fmt, ...)
{
    int ret = 0;
    va_list args;
    va_start(args, fmt);
    if (mverbose)
        ret = vfprintf(f, fmt, args);
    va_end(args);
    return ret;
}



int MathematicaL::initialize_connection(int new_name) 
{
    static char prefix[] = "WSTP (initialize):";
    if (mlp != (WSLINK)0)  // close existing link
    {
        deinitialize_close();
    }

    mep =  WSInitialize( (WSParametersPointer)0);
    if( mep == (WSENV)0) {
        fprintf_l(stderr, "%s failed to initialize WSENV\n", prefix);
        return 1;
    }

    if (new_name) { ++mcount; }
    // create linkname
    char linkname[15];
    snprintf(linkname, sizeof(linkname), "%s%u", mlink_prefix.c_str(), mcount);
    mlink_name = std::move(std::string{linkname});

    int err;

    const char *argv[5];
    argv[0] = "sample_ueye";
    argv[1] = "-linkname";
    argv[2] = linkname;
    argv[3] = "-linkcreate";
    argv[4] = nullptr;

    fprintf_l(stdout, "WSTP: listening on '%s' ...\n", linkname);
    mlp = WSOpenArgv(mep, (char**)argv, (char**)argv+4, &err);
    if (err == WSEOK) {
        fprintf_l(stdout, "WSTP: opened link\n");
    } else {
        fprintf_l(stderr, "%s failed to initialize WSLINK\n", prefix);
        return 2;
    }

    // register default Yield function to NOT block
    register_WSyield_fn(default_yield_fn);

    return activate_link();
}

int MathematicaL::register_WSyield_fn(const yield_fn_t &yfn)
{
    static const char prefix[] = "WSTP (register WSYield):";
    if (!yfn) return 1;

    int err = WSSetYieldFunction(mlp, yfn);
    if (!err) {
        fprintf_l(stderr, "%s unable to set yield function\n", prefix);
        return 3;
    }
    return 0;
}

int MathematicaL::activate_link()
{
    static const char prefix[] = "WSTP (activate_link):";
    state = WAITING_ACTIVATION;

    int ret = 1;
    int count = 0;
    while (true)
    {
        if (WSReady(mlp)) {
            if(WSActivate(mlp)) {
                state = ACTIVE_CONNECTION;
                ret = 0;
            } else {
                state = NO_ACTIVE_CONNECTION;
                fprintf_l(stderr, "%s failed to activate connection\n", prefix);
            }

            break;
        }

        // check to see if to continue polling
        if (yield_callback(WAITING_ACTIVATION, count++))
        {
            state = NO_ACTIVE_CONNECTION;
            break;
        }
    }

    return ret;
}

int MathematicaL::clear_error(LinkState state)
{
    if (!mlp || !mep) {
        fprintf_l(stderr, "WSTP (clear_erro): no connection to clear error\n");
        return 1;
    }

    if (state == VALID || state == DEFERRED)
    {
        return 0;
    }
    else if (state == ERROR) 
    {
        WSClearError(mlp);
        WSNewPacket(mlp);
        state = check_error();

        if (state != VALID) {
            return 1;
        } else  {
            return 0;
        }
    }

    return 1;
}



LinkState MathematicaL::check_error()
{
    const static char prefix[] = "WSTP (check_error):";
    int err = WSError(mlp);
    LinkState ret = ERROR;
    switch (err)
    {
        case WSEOK:
            ret = VALID;
            //fprintf(stderr, "%s link OK\n", prefix);
            break;
        case WSEDEAD:
            ret = INVALID;
            fprintf_l(stderr, "%s link died\n", prefix);
            break;
        case WSEGBAD:
            fprintf_l(stderr, "%s inconsistent data read\n", prefix);
            break;
        case WSEGSEQ:
            fprintf_l(stderr, "%s WSGet() function called out of sequence\n", prefix);
            break;
        case WSEPBTK:
            fprintf_l(stderr, "%s WSPutNext() passed a bad token\n", prefix);
            break;
        case WSEPSEQ:
            fprintf_l(stderr, "%s WSPut() function called out of sequence\n", prefix);
            break;
        case WSEPBIG:
            fprintf_l(stderr, "%s WSPutData() given too much data\n", prefix);
            break;
        case WSEOVFL:
            fprintf_l(stderr, "%s machine number overflow\n", prefix);
            break;
        case WSEMEM:
            fprintf_l(stderr, "%s out of memory\n", prefix);
            break;
        case WSEACCEPT:
            ret = INVALID;
            fprintf_l(stderr, "%s failure to accept socket connection\n", prefix);
            break;
        case WSECONNECT:
            ret = DEFERRED;
            //fprintf_l(stderr, "%s a deferred connection is still unconnected\n", prefix);
            break;
        case WSEPUTENDPACKET:
            fprintf_l(stderr, "%s unexpected or missing call of WSEndPacket()\n", prefix);
            break;
        case WSENEXTPACKET:
            fprintf_l(stderr, "%s WSNextPacket() called while the current packet has unread data\n", prefix);
            break;
        case WSEUNKNOWNPACKET:
            fprintf_l(stderr, "%s WSNextPacket() read in an unknown packet head\n", prefix);
            break;
        case WSEGETENDPACKET:
            fprintf_l(stderr, "%s unexpected end of packet\n", prefix);
            break;
        case WSEABORT:
            fprintf_l(stderr, "%s a put or get was aborted before affecting the link\n", prefix);
            break;
        case WSECLOSED:
            ret = CLOSED;
            fprintf_l(stderr, "%s the other side of the link closed the connection (you may still receive undelivered data)\n", prefix);
            break;
        case WSEINIT:
            fprintf_l(stderr, "%s the WSTP environment was not initialized\n", prefix);
            break;
        case WSEARGV:
            fprintf_l(stderr, "%s insufficient arguments to open the link\n", prefix);
            break;
        case WSEPROTOCOL:
            fprintf_l(stderr, "%s protocol unavailable\n", prefix);
            break;
        case WSEMODE:
            fprintf_l(stderr, "%s mode unavailable\n", prefix);
            break;
        case WSELAUNCH:
            fprintf_l(stderr, "%s launch unsupported\n", prefix);
            break;
        case WSELAUNCHAGAIN:
            fprintf_l(stderr, "%s cannot launch the program again from the same file\n", prefix);
            break;
        case WSELAUNCHSPACE:
            fprintf_l(stderr, "%s insufficient space to launch the program\n", prefix);
            break;
        case WSENOPARENT:
            fprintf_l(stderr, "%s no parent available for connection\n", prefix);
            break;
        case WSENAMETAKEN:
            fprintf_l(stderr, "%s the linkname was already in use\n", prefix);
            break;
        case WSENOLISTEN:
            fprintf_l(stderr, "%s the linkname was found not to be listening\n", prefix);
            break;
        case WSEBADNAME:
            fprintf_l(stderr, "%s the linkname was missing or not in the proper form\n", prefix);
            break;
        case WSEBADHOST:
            fprintf_l(stderr, "%s the location was unreachable or not in the proper form\n", prefix);
            break;
        case WSELAUNCHFAILED:
            fprintf_l(stderr, "%s the program failed to launch because a resource or library was missing\n", prefix);
            break;
        case WSELAUNCHNAME:
            fprintf_l(stderr, "%s the launch failed because the program could not be found\n", prefix);
            break;
        case WSEPSCONVERT:
            fprintf_l(stderr, "%s unable to convert from given character encoding to link encoding\n", prefix);
            break;
        case WSEGSCONVERT:
            fprintf_l(stderr, "%s unable to convert from link encoding to requested encoding\n", prefix);
            break;
        case WSEPDATABAD:
            fprintf_l(stderr, "%s character data in given encoding incorrect\n", prefix);
            break;
        case WSENOTEXE:
            fprintf_l(stderr, "%s specified file is not a WSTP executable\n", prefix);
            break;
        case WSESYNCOBJECTMAKE:
            fprintf_l(stderr, "%s unable to create WSTP synchronization object\n", prefix);
            break;
        case WSEBACKOUT:
            fprintf_l(stderr, "%s yield function terminated WSTP operation\n", prefix);
            break;
        case WSEBADOPTSYM:
            fprintf_l(stderr, "%s unable to recognize symbol value on link\n", prefix);
            break;
        case WSEBADOPTSTR:
            fprintf_l(stderr, "%s unable to recognize string value on link\n", prefix);
            break;
        case WSENEEDBIGGERBUFFER:
            fprintf_l(stderr, "%s function call needs bigger buffer argument\n", prefix);
            break;
        default:
            fprintf_l(stderr, "%s unkown error code\n", prefix);
            break;
    }

    return ret;
}

int MathematicaL::process_packet()
{
    const static char prefix[] =  "WSTP (process_packet):";
    State prev = state;

    // start polling for new packet
    state = WAITING_PACKET;
    printf("WSTP: waiting for packet ...\n");
    int count = 0;
    while (true) {
        WSFlush(mlp);
        if (!WSReady(mlp)) {
            break;
        }
        // check for error AND if to continue polling
        if (WSEOK != WSError(mlp)) {
            state = prev;
            return 1;
        }
        if (yield_callback && yield_callback(WAITING_PACKET, count++)) {
            state = WAITING_PACKET_YIELD;
            return 0;
        }
    }

    int ret = WSNextPacket(mlp);

    if (ret == ILLEGALPKT) {
        printf("%s illegal packet\n", prefix);
        return 1;
    }
    state = ACTIVE_CONNECTION;

    ret = WSGetType(mlp);
    if (ret == WSTKERROR) {
        printf("type pkt error\n");
        return 1;
    } else if (ret != WSTKFUNC) {
        fprintf_l(stderr, "%s received unkown type '%s', expected WSTKFUNC\n", prefix, str_WSType(ret));
        return !WSNewPacket(mlp);
    }

    const char *f;
    int n;
    if (!WSGetFunction(mlp, &f, &n)) {
        fprintf_l(stderr, "%s unable to read WSTKFUNC\n", prefix);
        return !WSNewPacket(mlp);
    }

    if (n < 1) {
        fprintf_l(stderr, "%s At least one argument to '%s'needed\n", prefix, f);
        WSReleaseSymbol(mlp, f);
        return !WSNewPacket(mlp);
    }

    WSReleaseSymbol(mlp, f);


    ret = WSGetType(mlp);
    if (ret != WSTKSTR) {
        fprintf_l(stderr, "%s received unkown type '%s', expected command as WSTKSTR\n", prefix, str_WSType(ret));
        return !WSNewPacket(mlp);
    }

    const char *command;
    ret = WSGetString(mlp, &command);
    if (!ret) {
        fprintf_l(stderr, "%s failed to read string\n", prefix);
        return !WSNewPacket(mlp);
    }

    if(!std::strcmp(command, "exposure")) 
    {
        if (n != 2) {
            fprintf_l(stderr, "%s exposure command needs 1 argument\n", prefix);
            return !WSNewPacket(mlp);
        }

        ret = command_exposure();
    }
    else if (!std::strcmp(command, "send"))
    {
        if (n != 2) {
            fprintf_l(stderr, "%s send command needs 1 argument\n", prefix);
            return !WSNewPacket(mlp);
        }
        ret = command_send();
    }
    else if (!std::strcmp(command, "imgcrop"))
    {
        if (n != 5) {
            fprintf_l(stderr, "%s imgcrop command needs 4 argument\n", prefix);
            return !WSNewPacket(mlp);
        }
        ret = command_imgcrop();
    }
    else if (!std::strcmp(command, "get"))
    {
        if (n != 1) {
            fprintf_l(stderr, "%s get command needs 0 arguments\n", prefix);
            return !WSNewPacket(mlp);
        }

        ret = command_get();
    } else if (!std::strcmp(command, "quit"))
    {
        if (n != 1) {
            fprintf_l(stderr, "%s quit command needs 0 arguments\n", prefix);
            return !WSNewPacket(mlp);
        }

        ret = -1;
    } else {
        fprintf_l(stderr, "%s unkown command '%s'\n", prefix, command);
    }

    WSReleaseString(mlp, command);

    return ret;
}

int MathematicaL::command_imgcrop() 
{
    int vals[4];

    for (size_t i = 0; i < 4; ++i)
    {
        int ret = WSGetType(mlp);
        if (ret != WSTKINT) {
            fprintf_l(stderr, "WSTP: received unkown type '%s' for %lu argument, expected integer\n", str_WSType(ret), i);
            return !WSNewPacket(mlp);
        }

        WSGetInteger(mlp, vals+i);
    }

    if (hook_imgcrop)
        hook_imgcrop(vals[0], vals[1], vals[2], vals[3]);

    return !WSNewPacket(mlp);
}

int MathematicaL::command_exposure() 
{
    int ret = WSGetType(mlp);
    if (ret != WSTKREAL) {
        fprintf_l(stderr, "WSTP: received unkown type '%s', expected floating point\n", str_WSType(ret));
        return !WSNewPacket(mlp);
    }

    double val;
    ret = WSGetReal(mlp, &val);
    if (hook_set_prop)
        hook_set_prop("exposure", val);
    return !WSNewPacket(mlp);
}

int MathematicaL::command_get()
{
    std::shared_ptr<Image> img {hook_get()};
    if (!img) {
        fprintf_l(stderr, "WSTP: failed to get image from camera\n");
        return 1;
    }

    const long dims[] {(long)img->h, (long)img->w, 3};
    // expand each 4-byte into 3 RGB values
    size_t N = img->h*img->w;
    int *data = new int[N*3]; // seems to be too large to allocate on stack - causes segfault
    rgb_to_rgb3(data, img->data, N, 3);

    fprintf_l(stdout, "WSTP: sending %lix%li\n", dims[1], dims[0]);

    WSPutIntegerArray(mlp, data, dims, NULL, 3); // note: blocks!
    WSEndPacket(mlp);
    delete[] data;

    return 0;
}

void rgb_to_rgb3(int *dst, const int *src, const size_t N, const size_t channels)
{
    int *ptr = dst;
    const int *src_ptr = src;
    const int *src_ptr_end = src+N;

    while (src_ptr != src_ptr_end)
    {
        for (size_t i = 0; i < channels; ++i)
        {
            *(ptr+i) = (((*src_ptr) >> (8*i)) & 0xff);
        }

        ptr += channels;
        ++src_ptr;
    }
}

void rgb3_to_rgb(int *dst, const int *src, const size_t N, const size_t channels)
{
    int *ptr = dst;
    const int *dst_ptr_end = dst+N;
    const int *src_ptr = src;
    while (ptr != dst_ptr_end)
    {
        *ptr = 0;
        for (size_t i = 0; i < channels; ++i)
        {
            *ptr |= ((*(src_ptr+i) & 0xff) << (i*8));
        }

        src_ptr += channels;
        ++ptr;
    }
}

void bw_to_rgb(int *dst, const int *src, const size_t N)
{
    int *ptr = dst;
    const int *dst_ptr_end = dst+N;
    const int *src_ptr = src;
    while (ptr != dst_ptr_end)
    {
        int val = (*src_ptr & 0xff);
        *ptr = ((val << 16) | (val << 8) | val);
        ++src_ptr;
        ++ptr;
    }
}


int MathematicaL::command_send()
{
    int ret = WSGetType(mlp);
    if (ret != WSTKFUNC) {
        fprintf_l(stderr, "WSTP: received unkown type '%s', expected WSTKFUNC\n", str_WSType(ret));
        return 1;
    }

    int *data;
    long *dims;
    char **heads;
    long d;

    ret = WSGetIntegerArray(mlp, &data, &dims, &heads, &d);
    if (!ret) {
        fprintf_l(stderr, "WSTP: unable to read integer array\n");
        return 1;
    }

    if (d != 2 && d != 3) {
        fprintf_l(stderr, "WSTP: dimension of array is not two or three, given %ld\n", d);
        WSReleaseIntegerArray(mlp, data, dims, heads, d);
        return 1;
    }

    if (hook_send)
    {
        const size_t N = dims[0]*dims[1];
        int *cdata = new int[N];
        if (d == 2) {
            // gray scale image
            bw_to_rgb(cdata, data, N);
        } else {
            // rgb image
            rgb3_to_rgb(cdata, data, N, 3);
        }

        // cdata memory will be release by ~Image
        std::shared_ptr<Image> img {std::make_shared<Image>(cdata, dims[1], dims[0])};
        hook_send(img);
    }

    WSReleaseIntegerArray(mlp, data, dims, heads, d);
    return 0;
}


MathematicaL::~MathematicaL() {
    MathematicaL::deinitialize_close();
    G_WSTP_PTR = nullptr;
}

void MathematicaL::deinitialize_close()
{
    // sometimes throws bad_alloc, idk why
    try {
        if (mlp != (WSLINK)0) {
            WSClose(mlp);
            mlp = 0;
        }
    } catch (std::bad_alloc &ba)
    {
        fprintf_l(stderr, "WSTP (deinitialize): failed to close link\n");
        mlp = 0;
    }

    try {
        if (mep != (WSENV)0) {
            WSDeinitialize(mep);
            mep = 0;
        }
    } catch (std::bad_alloc &ba)
    {
        fprintf_l(stderr, "WSTP (deinitialize): failed to deinitialize WSENV\n");
        mep = 0;
    }

    state = NO_ACTIVE_CONNECTION;
}
