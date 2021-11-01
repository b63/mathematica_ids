#include <cstdio>
#include <wstp.h>
#include <cstring>
#include <array>
#include <string>

#include <chrono>
#include <random>
#include <memory>
#include <thread>

#include <wstp.h>

#include "MathematicaL.h"


void print_char(char **arr, int n);
void print_int(int *arr, int n);

std::shared_ptr<Image> camera_img()
{
    int w = 5, h = 10;
    std::shared_ptr<Image> dst {std::make_shared<Image>(nullptr, w, h)};
    
    static std::default_random_engine engine;
    static std::uniform_int_distribution<int> dist(0, 255);
    
    dst->data = new int[w*h];

    for (int i = 0; i < w*h; ++i)
        dst->data[i] = dist(engine);

    return dst;
}

int yield_block(State state, int count)
{
    if (state == WAITING_ACTIVATION) {
        if (count < 10) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return 0;
        }
        return 1;
    } else if (state == WAITING_PACKET) {
        if (count < 5) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return 0;
        }
        return 1;
    }

    return 1;
}

int main(int argc, char * argv[]) 
{
    MathematicaL mlink {"ueye_", camera_img, nullptr, nullptr, yield_block};
    mlink.initialize_connection();

    int ret;
    int tries = 0;
    while (true)
    {
        LinkState state = mlink.check_error();
        if (state == DEFERRED) {
            printf("waiting for connection ... \n");
            mlink.activate_link();
            continue;
        }

        ret = mlink.clear_error(state);
        if (ret) {
            if (tries < 2 ) {
                fprintf(stderr, "main: unable to clear error, starting another link\n");
                mlink.deinitialize_close();
                mlink.initialize_connection();
                tries++;
            } else {
                fprintf(stderr, "tried %i times, giving up\n", tries);
                break;
            }
        } else {
            while (1)
            {
                ret = mlink.process_packet();
                if (mlink.get_state() != WAITING_PACKET_YIELD) {
                    break;
                }
                printf("waiting for packet again?\n");
            }

            if (ret == -1) {
                break;
            } else if (!ret) {
                tries = 0;
            }
        }
    }
    printf("main: quitting\n");
}
// */

/**
static void init_and_openlink( int argc, char* argv[]);
static void error( WSLINK lp);
std::array<char*, 256> PKT_STRING{};
WSENV ep = (WSENV)0;
WSLINK lp = (WSLINK)0;

static void error( WSLINK lp)
{
    if( WSError( lp)){
        fprintf( stderr, "Error detected by WSTP: %s.\n",
                WSErrorMessage(lp));
    }else{
        fprintf( stderr, "Error detected by this program.\n");
    }
    exit(3);
}


static void deinit( void)
{
    if( ep) WSDeinitialize( ep);
}


static void closelink( void)
{
    if( lp) WSClose( lp);
}

static int print_type(int type) {
    int ret = 1;
    switch (type) {
        case WSTKERR:
            printf("WSTKERR");
            ret = 1;
            break;
        case WSTKINT:
            printf("WSKINT");
            break;
        case WSTKFUNC:
            printf("WSTKFUNC");
            break;
        case WSTKREAL:
            printf("WSTKREAL");
            break;
        case WSTKSTR:
            printf("WSTKSTR");
            break;
        case WSTKSYM:
            printf("WSTKSYM");
            break;
        case WSTKOLDINT:
            printf("WSTKOLDINT");
            break;
        case WSTKOLDREAL:
            printf("WSTKOLDREAL");
            break;
        case WSTKOLDSTR:
            printf("WSTKOLDSTR");
            break;
        case WSTKOLDSYM:
            printf("WSTKOLDSYM");
            break;
        case WSTKOPTSTR:
            printf("WSTKOPTSTR");
            break;
        case WSTKOPTSYM:
            printf("WSTKOPTSYM");
            break;
    }
    return ret;
}



static void init_and_openlink( int argc, char* argv[])
{
    int err;
    printf("argc: %i\n",argc);
    char **ptr = argv;
    while (*ptr) {
        printf("'%s'", *ptr);
        ptr++;
    }

    ep =  WSInitialize( (WSParametersPointer)0);
    if( ep == (WSENV)0) exit(1);
    atexit( deinit);

    lp = WSOpenArgv( ep, argv, argv + argc, &err);
    if(lp == (WSLINK)0) exit(2);
    atexit( closelink);	
}

void create_pktstring() {
    PKT_STRING[ILLEGALPKT]     = "ILLEGALPKT";
    PKT_STRING[CALLPKT]        = "CALLPKT";
    PKT_STRING[EVALUATEPKT]    = "EVALUATEPKT";
    PKT_STRING[RETURNPKT]      = "RETURNPKT";
    PKT_STRING[INPUTNAMEPKT]   = "INPUTNAMEPKT";
    PKT_STRING[ENTERTEXTPKT]   = "ENTERTEXTPKT";
    PKT_STRING[ENTEREXPRPKT]   = "ENTEREXPRPKT";
    PKT_STRING[OUTPUTNAMEPKT]  = "OUTPUTNAMEPKT";
    PKT_STRING[RETURNTEXTPKT]  = "RETURNTEXTPKT";
    PKT_STRING[RETURNEXPRPKT]  = "RETURNEXPRPKT";
    PKT_STRING[DISPLAYPKT]     = "DISPLAYPKT";
    PKT_STRING[DISPLAYENDPKT]  = "DISPLAYENDPKT";
    PKT_STRING[MESSAGEPKT]     = "MESSAGEPKT";
    PKT_STRING[TEXTPKT]        = "TEXTPKT";
    PKT_STRING[INPUTPKT]       = "INPUTPKT";
    PKT_STRING[INPUTSTRPKT]    = "INPUTSTRPKT";
    PKT_STRING[MENUPKT]        = "MENUPKT";
    PKT_STRING[SYNTAXPKT]      = "SYNTAXPKT";
    PKT_STRING[SUSPENDPKT]     = "SUSPENDPKT";
    PKT_STRING[RESUMEPKT]      = "RESUMEPKT";
    PKT_STRING[BEGINDLGPKT]    = "BEGINDLGPKT";
    PKT_STRING[ENDDLGPKT]      = "ENDDLGPKT";
    PKT_STRING[FIRSTUSERPKT]   = "FIRSTUSERPKT";
    PKT_STRING[LASTUSERPKT]    = "LASTUSERPKT";
}

int main(int argc, char **argv) {
    printf("%i\n",argc);
    create_pktstring();

    init_and_openlink( argc, argv);

    printf("up here!\n\r");
    WSPutString(lp, "Hello!");
    printf("here!\n\r");
    WSEndPacket(lp);
    printf("sent packet\n");

    int rt = 0;
    const char *resp = 0;

    int *a;
    int *dims;
    char **heads;
    int d;

    const char *sym;
    int n;

    while (1)
    {
        rt = WSNextPacket(lp);
        printf("new packet: %s\n", PKT_STRING[rt]);
        if (!rt) {
            const char *msg = WSErrorMessage(lp);
            printf("error: %s\n", msg);
            if (strcmp(msg, "WSTP connnection was lost.")) {
                break;
            }

            WSClearError(lp);
        } else {
            printf("type: ");
            int type = WSGetType(lp);
            print_type(type);
            printf("\n");

            if (type == WSTKSTR) {
                if(WSGetString(lp, &resp)){
                    printf("response: %s\n", resp);
                    if (!strcmp(resp, "quit")) {
                        WSReleaseString(lp, resp);
                        break;
                    } else if (!strcmp(resp, "array")) {
                        int sub1[3] = {1, 2, 3};
                        int sub2[3] = {2, 4, 6};
                        int sub3[3] = {4, 8, 16};
                        int *_a[3] = {sub1, sub3, sub2};
                        int _dims[]=  {3, 3};
                        print_int((int*)_a, 3);
                        printf("\n");
                        WSPutInteger32Array(lp, (int*)_a, _dims, NULL, 2);
                        WSEndPacket(lp);

                    }

                    if (resp) WSReleaseString(lp, resp);
                } else {
                    printf("failed to read string\n");
                }
            } else if (type == WSTKFUNC) {
                if(WSGetInteger32Array(lp, &a, &dims, &heads, &d)) {
                    printf("got array size %i: ", d);
                    print_char(heads, d);
                    printf("dims: ");
                    print_int(dims, d);
                    WSReleaseInteger32Array(lp, a, dims, heads, d);
                } else if (WSGetFunction(lp, &sym, &n)){
                    printf("got: %s, %i\n", sym, n);
                    WSReleaseSymbol(lp, sym);
                }
            } else {
                printf("got something else\n");
            }
        }
    }

    closelink();
    deinit();
    return 0;
}
// */

void print_char(char **arr, int n) {
    printf("{");
    while (n--) {
        printf("%s", *(arr++));
        if (n) {
            printf(", ");
        }
    }
    printf("}\n");
}

void print_int(int *arr, int n) {
    printf("{");
    while (n--) {
        printf("%i", *(arr++));
        if (n) {
            printf(", ");
        }
    }
    printf("}\n");
}

