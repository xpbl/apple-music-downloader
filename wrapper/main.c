#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "import.h"
#include "cmdline.h"
#ifndef MyRelease
#include "subhook/subhook.c"
#endif

static struct shared_ptr apInf;
static uint8_t leaseMgr[16];
struct gengetopt_args_info args_info;
char *amUsername, *amPassword;

int file_exists(char *filename) {
  struct stat buffer;   
  return (stat (filename, &buffer) == 0);
}

static void dialogHandler(long j, struct shared_ptr *protoDialogPtr,
                          struct shared_ptr *respHandler) {
    const char *const title = std_string_data(
        _ZNK17storeservicescore14ProtocolDialog5titleEv(protoDialogPtr->obj));
    fprintf(stderr, "[.] dialogHandler: {title: %s, message: %s}\n", title,
            std_string_data(_ZNK17storeservicescore14ProtocolDialog7messageEv(
                protoDialogPtr->obj)));

    unsigned char ptr[72];
    memset(ptr + 8, 0, 16);
    *(void **)(ptr) =
        &_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore22ProtocolDialogResponseENS_9allocatorIS2_EEEE +
        2;
    struct shared_ptr diagResp = {.obj = ptr + 24, .ctrl_blk = ptr};
    _ZN17storeservicescore22ProtocolDialogResponseC1Ev(diagResp.obj);

    struct std_vector *butVec =
        _ZNK17storeservicescore14ProtocolDialog7buttonsEv(protoDialogPtr->obj);
    if (strcmp("Sign In", title) == 0) {
        for (struct shared_ptr *b = butVec->begin; b != butVec->end; ++b) {
            if (strcmp("Use Existing Apple ID",
                       std_string_data(
                           _ZNK17storeservicescore14ProtocolButton5titleEv(
                               b->obj))) == 0) {
                _ZN17storeservicescore22ProtocolDialogResponse17setSelectedButtonERKNSt6__ndk110shared_ptrINS_14ProtocolButtonEEE(
                    diagResp.obj, b);
                break;
            }
        }
    } else {
        for (struct shared_ptr *b = butVec->begin; b != butVec->end; ++b) {
            fprintf(
                stderr, "[.] button %p: %s\n", b->obj,
                std_string_data(
                    _ZNK17storeservicescore14ProtocolButton5titleEv(b->obj)));
        }
    }
    _ZN20androidstoreservices28AndroidPresentationInterface28handleProtocolDialogResponseERKlRKNSt6__ndk110shared_ptrIN17storeservicescore22ProtocolDialogResponseEEE(
        apInf.obj, &j, &diagResp);
}

static void credentialHandler(struct shared_ptr *credReqHandler,
                              struct shared_ptr *credRespHandler) {
    const uint8_t need2FA =
        _ZNK17storeservicescore18CredentialsRequest28requiresHSA2VerificationCodeEv(
            credReqHandler->obj);
    fprintf(
        stderr, "[.] credentialHandler: {title: %s, message: %s, 2FA: %s}\n",
        std_string_data(_ZNK17storeservicescore18CredentialsRequest5titleEv(
            credReqHandler->obj)),
        std_string_data(_ZNK17storeservicescore18CredentialsRequest7messageEv(
            credReqHandler->obj)),
        need2FA ? "true" : "false");

    int passLen = strlen(amPassword);

    if (need2FA) {
        if (args_info.code_from_file_flag) {
            fprintf(stderr, "[!] Enter your 2FA code into rootfs/data/code.txt\n");
            fprintf(stderr, "[!] Example command: echo -n 114514 > rootfs/data/2fa.txt\n");
            fprintf(stderr, "[!] Waiting for input...\n");
            int count = 0;
            while (1)
            {
                if (count >= 20) {
                    fprintf(stderr, "[!] Failed to get 2FA Code in 60s. Exiting...\n");
                    exit(0);
                }
                if (file_exists("/data/2fa.txt")) {
                    FILE *fp = fopen("/data/2fa.txt", "r");
                    fscanf(fp, "%6s", amPassword + passLen);
                    remove("/data/2fa.txt");
                    fprintf(stderr, "[!] Code file detected! Logging in...\n");
                    break;
                } else {
                    sleep(3);
                    count++;
                }
            }
        } else {
            printf("2FA code: ");
            scanf("%6s", amPassword + passLen);
        }
    }

    uint8_t *const ptr = malloc(80);
    memset(ptr + 8, 0, 16);
    *(void **)(ptr) =
        &_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore19CredentialsResponseENS_9allocatorIS2_EEEE +
        2;
    struct shared_ptr credResp = {.obj = ptr + 24, .ctrl_blk = ptr};
    _ZN17storeservicescore19CredentialsResponseC1Ev(credResp.obj);

    union std_string username = new_std_string(amUsername);
    _ZN17storeservicescore19CredentialsResponse11setUserNameERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        credResp.obj, &username);

    union std_string password = new_std_string(amPassword);
    _ZN17storeservicescore19CredentialsResponse11setPasswordERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        credResp.obj, &password);

    _ZN17storeservicescore19CredentialsResponse15setResponseTypeENS0_12ResponseTypeE(
        credResp.obj, 2);

    _ZN20androidstoreservices28AndroidPresentationInterface25handleCredentialsResponseERKNSt6__ndk110shared_ptrIN17storeservicescore19CredentialsResponseEEE(
        apInf.obj, &credResp);
}

#ifndef MyRelease
static uint8_t allDebug() { return 1; }
#endif

static inline void init() {
    // srand(time(0));

    // raise(SIGSTOP);
    fprintf(stderr, "[+] starting...\n");
    setenv("ANDROID_DNS_MODE", "local", 1);
    if (args_info.proxy_given) {
        fprintf(stderr, "[+] Using proxy %s", args_info.proxy_arg);
        setenv("http_proxy", args_info.proxy_arg, 1);
        setenv("https_proxy", args_info.proxy_arg, 1);
    }

    static const char *resolvers[2] = {"1.1.1.1", "1.0.0.1"};
    _resolv_set_nameservers_for_net(0, resolvers, 2, ".");
#ifndef MyRelease
    subhook_install(subhook_new(
        _ZN13mediaplatform26DebugLogEnabledForPriorityENS_11LogPriorityE,
        allDebug, SUBHOOK_64BIT_OFFSET));
#endif

    // static char android_id[16];
    // for (int i = 0; i < 16; ++i) {
    //     android_id[i] = "0123456789abcdef"[rand() % 16];
    // }
    union std_string conf1 = new_std_string(android_id);
    union std_string conf2 = new_std_string("");
    _ZN14FootHillConfig6configERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE(
        &conf1);

    // union std_string root = new_std_string("/");
    // union std_string natLib = new_std_string("/system/lib64/");
    // void *foothill = malloc(120);
    // _ZN8FootHillC2ERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_(
    //     foothill, &root, &natLib);
    // _ZN8FootHill24defaultContextIdentifierEv(foothill);

    struct shared_ptr GUID;
    _ZN17storeservicescore10DeviceGUID8instanceEv(&GUID);

    static uint8_t ret[88];
    static unsigned int conf3 = 29;
    static uint8_t conf4 = 1;
    _ZN17storeservicescore10DeviceGUID9configureERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_RKjRKb(
        &ret, GUID.obj, &conf1, &conf2, &conf3, &conf4);
}

static inline struct shared_ptr init_ctx() {
    fprintf(stderr, "[+] initializing ctx...\n");
    union std_string strBuf =
        new_std_string("/data/data/com.apple.android.music/files/mpl_db");

    struct shared_ptr reqCtx;
    _ZNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEE11make_sharedIJRNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEEEES3_DpOT_(
        &reqCtx, &strBuf);

    static uint8_t ptr[480];
    *(void **)(ptr) =
        &_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore20RequestContextConfigENS_9allocatorIS2_EEEE +
        2;
    struct shared_ptr reqCtxCfg = {.obj = ptr + 32, .ctrl_blk = ptr};

    _ZN17storeservicescore20RequestContextConfigC2Ev(reqCtxCfg.obj);
	// _ZN17storeservicescore20RequestContextConfig9setCPFlagEb(reqCtx.obj, 1);
    _ZN17storeservicescore20RequestContextConfig20setBaseDirectoryPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        reqCtxCfg.obj, &strBuf);
    strBuf = new_std_string("Music");
    _ZN17storeservicescore20RequestContextConfig19setClientIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        reqCtxCfg.obj, &strBuf);
    strBuf = new_std_string("4.9");
    _ZN17storeservicescore20RequestContextConfig20setVersionIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        reqCtxCfg.obj, &strBuf);
    strBuf = new_std_string("Android");
    _ZN17storeservicescore20RequestContextConfig21setPlatformIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        reqCtxCfg.obj, &strBuf);
    strBuf = new_std_string("10");
    _ZN17storeservicescore20RequestContextConfig17setProductVersionERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        reqCtxCfg.obj, &strBuf);
    strBuf = new_std_string("Samsung S9");
    _ZN17storeservicescore20RequestContextConfig14setDeviceModelERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        reqCtxCfg.obj, &strBuf);
    strBuf = new_std_string("7663313");
    _ZN17storeservicescore20RequestContextConfig15setBuildVersionERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        reqCtxCfg.obj, &strBuf);
    strBuf = new_std_string("en-US");
    _ZN17storeservicescore20RequestContextConfig19setLocaleIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        reqCtxCfg.obj, &strBuf);
    _ZN17storeservicescore20RequestContextConfig21setLanguageIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        reqCtxCfg.obj, &strBuf);

    _ZN21RequestContextManager9configureERKNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEEE(
        &reqCtx);
    static uint8_t buf[88];
    _ZN17storeservicescore14RequestContext4initERKNSt6__ndk110shared_ptrINS_20RequestContextConfigEEE(
        &buf, reqCtx.obj, &reqCtxCfg);
    strBuf = new_std_string("/data/data/com.apple.android.music/files");
    _ZN17storeservicescore14RequestContext24setFairPlayDirectoryPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
        reqCtx.obj, &strBuf);

    _ZNSt6__ndk110shared_ptrIN20androidstoreservices28AndroidPresentationInterfaceEE11make_sharedIJEEES3_DpOT_(
        &apInf);

    _ZN20androidstoreservices28AndroidPresentationInterface16setDialogHandlerEPFvlNSt6__ndk110shared_ptrIN17storeservicescore14ProtocolDialogEEENS2_INS_36AndroidProtocolDialogResponseHandlerEEEE(
        apInf.obj, &dialogHandler);

    _ZN20androidstoreservices28AndroidPresentationInterface21setCredentialsHandlerEPFvNSt6__ndk110shared_ptrIN17storeservicescore18CredentialsRequestEEENS2_INS_33AndroidCredentialsResponseHandlerEEEE(
        apInf.obj, &credentialHandler);

    _ZN17storeservicescore14RequestContext24setPresentationInterfaceERKNSt6__ndk110shared_ptrINS_21PresentationInterfaceEEE(
        reqCtx.obj, &apInf);

    return reqCtx;
}

extern void *endLeaseCallback;
extern void *pbErrCallback;

inline static uint8_t login(struct shared_ptr reqCtx) {
    fprintf(stderr, "[+] logging in...\n");
    struct shared_ptr flow;
    _ZNSt6__ndk110shared_ptrIN17storeservicescore16AuthenticateFlowEE11make_sharedIJRNS0_INS1_14RequestContextEEEEEES3_DpOT_(
        &flow, &reqCtx);
    _ZN17storeservicescore16AuthenticateFlow3runEv(flow.obj);
    struct shared_ptr *resp =
        _ZNK17storeservicescore16AuthenticateFlow8responseEv(flow.obj);
    if (resp == NULL || resp->obj == NULL)
        return 0;
    const int respType =
        _ZNK17storeservicescore20AuthenticateResponse12responseTypeEv(
            resp->obj);
    fprintf(stderr, "[.] response type %d\n", respType);
    return respType == 6;
    // struct shared_ptr subStatMgr;
    // _ZN20androidstoreservices30SVSubscriptionStatusMgrFactory6createEv(&subStatMgr);
    // struct shared_ptr data;
    // int method = 2;
    // _ZN20androidstoreservices27SVSubscriptionStatusMgrImpl33checkSubscriptionStatusFromSourceERKNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEEERKNS_23SVSubscriptionStatusMgr26SVSubscriptionStatusSourceE(&data,
    // subStatMgr.obj, &reqCtx, &method);
    // return 1;
}

static inline uint8_t readfull(const int connfd, void *const buf,
                               const size_t size) {
    size_t red = 0;
    while (size > red) {
        const ssize_t b = read(connfd, ((uint8_t *)buf) + red, size - red);
        if (b <= 0)
            return 0;
        red += b;
    }
    return 1;
}

static inline void writefull(const int connfd, void *const buf,
                             const size_t size) {
    size_t red = 0;
    while (size > red) {
        const ssize_t b = write(connfd, ((uint8_t *)buf) + red, size - red);
        if (b <= 0) {
            perror("write");
            break;
        }
        red += b;
    }
}

static void *FHinstance = NULL;
static void *preshareCtx = NULL;

inline static void *getKdContext(const char *const adam,
                                 const char *const uri) {
    uint8_t isPreshare = (strcmp("0", adam) == 0);
    if (isPreshare && preshareCtx != NULL) {
        return preshareCtx;
    }
    fprintf(stderr, "[.] adamId: %s, uri: %s\n", adam, uri);

    union std_string defaultId = new_std_string(adam);
    union std_string keyUri = new_std_string(uri);
    union std_string keyFormat =
        new_std_string("com.apple.streamingkeydelivery");
    union std_string keyFormatVer = new_std_string("1");
    union std_string serverUri = new_std_string(
        "https://play.itunes.apple.com/WebObjects/MZPlay.woa/music/fps");
    union std_string protocolType = new_std_string("simplified");
    union std_string fpsCert = new_std_string(fairplayCert);

    struct shared_ptr persistK = {.obj = NULL};
    _ZN21SVFootHillSessionCtrl16getPersistentKeyERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_S8_S8_S8_S8_S8_(
        &persistK, FHinstance, &defaultId, &defaultId, &keyUri, &keyFormat,
        &keyFormatVer, &serverUri, &protocolType, &fpsCert);

    if (persistK.obj == NULL)
        return NULL;

    struct shared_ptr SVFootHillPContext;
    _ZN21SVFootHillSessionCtrl14decryptContextERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEERKN11SVDecryptor15SVDecryptorTypeERKb(
        &SVFootHillPContext, FHinstance, persistK.obj);

    if (SVFootHillPContext.obj == NULL)
        return NULL;

    void *kdContext =
        *_ZNK18SVFootHillPContext9kdContextEv(SVFootHillPContext.obj);
    if (kdContext != NULL && isPreshare)
        preshareCtx = kdContext;
    return kdContext;
}

void handle(const int connfd) {
    while (1) {
        uint8_t adamSize;
        if (!readfull(connfd, &adamSize, sizeof(uint8_t)))
            return;
        if (adamSize <= 0)
            return;

        char adam[adamSize + 1];
        if (!readfull(connfd, adam, adamSize))
            return;
        adam[adamSize] = '\0';

        uint8_t uri_size;
        if (!readfull(connfd, &uri_size, sizeof(uint8_t)))
            return;

        char uri[uri_size + 1];
        if (!readfull(connfd, uri, uri_size))
            return;
        uri[uri_size] = '\0';

        void **const kdContext = getKdContext(adam, uri);
        if (kdContext == NULL)
            return;

        while (1) {
            uint32_t size;
            if (!readfull(connfd, &size, sizeof(uint32_t))) {
                perror("read");
                return;
            }

            if (size <= 0)
                break;

            void *sample = malloc(size);
            if (sample == NULL) {
                perror("malloc");
                return;
            }
            if (!readfull(connfd, sample, size)) {
                free(sample);
                perror("read");
                return;
            }

            NfcRKVnxuKZy04KWbdFu71Ou(*kdContext, 5, sample, sample, size);
            writefull(connfd, sample, size);
            free(sample);
        }
    }
}

extern uint8_t handle_cpp(int);

inline static int new_socket() {
    const int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    if (fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }
    const int optval = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    static struct sockaddr_in serv_addr = {.sin_family = AF_INET};
    inet_pton(AF_INET, args_info.host_arg, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(args_info.decrypt_port_arg);
    if (bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(fd, 5) == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }

    fprintf(stderr, "[!] listening %s:%d\n", args_info.host_arg, args_info.decrypt_port_arg);
    // close(STDOUT_FILENO);

    static struct sockaddr_in peer_addr;
    static socklen_t peer_addr_size = sizeof(peer_addr);
    while (1) {
        const int connfd = accept4(fd, (struct sockaddr *)&peer_addr,
                                   &peer_addr_size, SOCK_CLOEXEC);
        if (connfd == -1) {
            if (errno == ENETDOWN || errno == EPROTO || errno == ENOPROTOOPT ||
                errno == EHOSTDOWN || errno == ENONET ||
                errno == EHOSTUNREACH || errno == EOPNOTSUPP ||
                errno == ENETUNREACH)
                continue;
            perror("accept4");
            return EXIT_FAILURE;
        }

        if (!handle_cpp(connfd)) {
            uint8_t autom = 1;
            _ZN22SVPlaybackLeaseManager12requestLeaseERKb(leaseMgr, &autom);
        }
        // if (sigsetjmp(catcher.env, 0) == 0) {
        //     catcher.do_jump = 1;
        //     handle(connfd);
        // }
        // catcher.do_jump = 0;

        if (close(connfd) == -1) {
            perror("close");
            return EXIT_FAILURE;
        }
    }
}


const char* get_m3u8_method_play(uint8_t leaseMgr[16], unsigned long adam) {
    union std_string HLS = new_std_string_short_mode("HLS");
    struct std_vector HLSParam = new_std_vector(&HLS);
    static uint8_t z0 = 0;
    struct shared_ptr ptr_result;
    _ZN22SVPlaybackLeaseManager12requestAssetERKmRKNSt6__ndk16vectorINS2_12basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEENS7_IS9_EEEERKb(
        &ptr_result, leaseMgr, &adam, &HLSParam, &z0
    );
    
    if (ptr_result.obj == NULL) {
        return NULL;
    }

    if (_ZNK23SVPlaybackAssetResponse13hasValidAssetEv(ptr_result.obj)) {
        struct shared_ptr *playbackAsset = _ZNK23SVPlaybackAssetResponse13playbackAssetEv(ptr_result.obj);
        if (playbackAsset == NULL || playbackAsset->obj == NULL) {
            return NULL;
        }

        union std_string *m3u8 = malloc(sizeof(union std_string));
        if (m3u8 == NULL) {
            return NULL;
        }

        void *playbackObj = playbackAsset->obj;
        _ZNK17storeservicescore13PlaybackAsset9URLStringEv(m3u8, playbackObj);

        if (m3u8 == NULL || std_string_data(m3u8) == NULL) {
            free(m3u8);
            return NULL;
        }
        
        const char *m3u8_str = std_string_data(m3u8);
        if (m3u8_str) {
            char *result = strdup(m3u8_str);  // Make a copy
            free(m3u8);
            return result;
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

void handle_m3u8(const int connfd) {
    while (1)
    {
        uint8_t adamSize;
        if (!readfull(connfd, &adamSize, sizeof(uint8_t))) {
            return;
        }
        if (adamSize <= 0) {
            return;
        }
        char adam[adamSize];
        for (int i=0; i<adamSize; i=i+1) {
            readfull(connfd, &adam[i], sizeof(uint8_t));
        }
        char *ptr;
        unsigned long adamID = strtoul(adam, &ptr, 10);
        const char *m3u8 = get_m3u8_method_play(leaseMgr, adamID);
        if (m3u8 == NULL) {
            fprintf(stderr, "[.] failed to get m3u8 of adamId: %ld\n", adamID);
            writefull(connfd, "\n", sizeof("\n"));
        } else {
            fprintf(stderr, "[.] m3u8 adamId: %ld, url: %s\n", adamID, m3u8);
            char *with_newline = malloc(strlen(m3u8) + 2);
            if (with_newline) {
                strcpy(with_newline, m3u8);
                strcat(with_newline, "\n");
                writefull(connfd, with_newline, strlen(with_newline));
                free(with_newline);
            }
            free((void *)m3u8);
        }
    }
}

static inline void *new_socket_m3u8(void *args) {
    const int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    if (fd == -1) {
        perror("socket");
    }
    const int optval = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    static struct sockaddr_in serv_addr = {.sin_family = AF_INET};
    inet_pton(AF_INET, args_info.host_arg, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(args_info.m3u8_port_arg);
    if (bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("bind");
    }

    if (listen(fd, 5) == -1) {
        perror("listen");
    }

    fprintf(stderr, "[!] listening m3u8 request on %s:%d\n", args_info.host_arg, args_info.m3u8_port_arg);
    // close(STDOUT_FILENO);

    static struct sockaddr_in peer_addr;
    static socklen_t peer_addr_size = sizeof(peer_addr);
    while (1) {
        const int connfd = accept4(fd, (struct sockaddr *)&peer_addr,
                                   &peer_addr_size, SOCK_CLOEXEC);
        if (connfd == -1) {
            if (errno == ENETDOWN || errno == EPROTO || errno == ENOPROTOOPT ||
                errno == EHOSTDOWN || errno == ENONET ||
                errno == EHOSTUNREACH || errno == EOPNOTSUPP ||
                errno == ENETUNREACH)
                continue;
            perror("accept4");
            
        }

        handle_m3u8(connfd);

        if (close(connfd) == -1) {
            perror("close");
        }
    }
}

int main(int argc, char *argv[]) {
    cmdline_parser(argc, argv, &args_info);

    init();
    const struct shared_ptr ctx = init_ctx();
    if (args_info.login_given) {
        amUsername = strtok(args_info.login_arg, ":");
        amPassword = strtok(NULL, ":");
    }
    if (args_info.login_given && !login(ctx)) {
        fprintf(stderr, "[!] login failed\n");
        return EXIT_FAILURE;
    }
    _ZN22SVPlaybackLeaseManagerC2ERKNSt6__ndk18functionIFvRKiEEERKNS1_IFvRKNS0_10shared_ptrIN17storeservicescore19StoreErrorConditionEEEEEE(
        leaseMgr, &endLeaseCallback, &pbErrCallback);
    uint8_t autom = 1;
    _ZN22SVPlaybackLeaseManager25refreshLeaseAutomaticallyERKb(leaseMgr,
                                                               &autom);
    _ZN22SVPlaybackLeaseManager12requestLeaseERKb(leaseMgr, &autom);
    FHinstance = _ZN21SVFootHillSessionCtrl8instanceEv();

    pthread_t m3u8_thread;
    pthread_create(&m3u8_thread, NULL, &new_socket_m3u8, NULL);
    pthread_detach(m3u8_thread);
    
    return new_socket();
}
