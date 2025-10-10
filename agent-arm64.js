'use strict';

const fairplayCert = /* fairplay-1.crt */;
const kdContextMap = new Map();
let persistentKeyPtr = null;

function newStdStringFromArrayBuffer(content) {
    return newStdString(String.fromCharCode(...new Uint8Array(content)));
}

function newStdString(content) {
    const size = content.length;
    if (size >= 0x17) {
        const capacity = 2 ** Math.ceil(Math.log2(size + 1));
        const buffer = malloc(capacity);
        buffer.writeUtf8String(content);

        const str = malloc(Process.pointerSize * 3);
        str.writeULong(capacity | 0x1);
        str.add(Process.pointerSize).writeULong(size);
        str.add(Process.pointerSize * 2).writePointer(buffer);

        return { buffer, str };
    } else {
        const str = malloc(size + 2);
        str.writeU8(size * 2);
        str.add(1).writeUtf8String(content);
        str.add(size + 1).writeU8(0);

        return { buffer: null, str };
    }
}

function getStrFromStdString(content) {
    const mem = new NativePointer(content);
    const size = mem.readU8();
    if ((size & 0x1) === 1) {
        const bufferSize = mem.add(Process.pointerSize).readULong();
        const bufferPtr = mem.add(Process.pointerSize * 2).readPointer();
        return bufferPtr.readUtf8String(bufferSize);
    } else {
        return mem.add(1).readUtf8String(size / 2);
    }
}

function getPersistentKeyASM(
    sessionCtrlInstance,
    adamIdStr, keyUriStr,
    keyFormatStr, keyFormatVerStr,
    serverUriStr, protocolTypeStr,
    fpsCertStr, persistentKeyPtr
) {
    const impl = malloc(Process.pageSize);
    Memory.patchCode(impl, Process.pageSize, code => {
        const writer = new Arm64Writer(code, { pc: impl });
        writer.putLdrRegAddress("x0", sessionCtrlInstance);
        writer.putLdrRegAddress("x1", adamIdStr.str);
        writer.putLdrRegAddress("x2", adamIdStr.str);
        writer.putLdrRegAddress("x3", keyUriStr.str);
        writer.putLdrRegAddress("x4", keyFormatStr.str);
        writer.putLdrRegAddress("x5", keyFormatVerStr.str);
        writer.putLdrRegAddress("x6", serverUriStr.str);
        writer.putLdrRegAddress("x7", protocolTypeStr.str);
        writer.putLdrRegAddress("x8", persistentKeyPtr);
        writer.putSubRegRegImm("sp", "sp", 0x10);
        writer.putLdrRegAddress("x9", fpsCertStr.str);
        writer.putStrRegRegOffset("x9", "sp", 0);
        writer.putCallAddressWithArguments(getPersistentKeyAddr, ["x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8"]);
        writer.putAddRegRegImm("sp", "sp", 0x10);
        writer.flush();
    });

    const implFunc = new NativeFunction(impl, 'void', []);
    try {
        implFunc();
    } catch (e) { } // Ignore errors
    dealloc(impl);
}

function decryptContextASM(sessionCtrlInstance, persistentKeyPtr, decryptedKeyPtr) {
    const impl = malloc(Process.pageSize);
    Memory.patchCode(impl, Process.pageSize, code => {
        const writer = new Arm64Writer(code, { pc: impl });
        writer.putLdrRegAddress("x0", sessionCtrlInstance);
        writer.putLdrRegAddress("x1", persistentKeyPtr);
        writer.putLdrRegAddress("x8", decryptedKeyPtr);
        writer.putCallAddressWithArguments(decryptContextAddr, ["x0", "x1", "x8"]);
        writer.flush();
    });

    const implFunc = new NativeFunction(impl, 'void', []);
    try {
        implFunc();
    } catch (e) { } // Ignore errors
    dealloc(impl);
}

function getKdContext(adamId, uri) {
    const uriStr = String.fromCharCode(...new Uint8Array(uri));
    if (kdContextMap.has(uriStr)) {
        return kdContextMap.get(uriStr);
    }

    const adamIdStr = newStdStringFromArrayBuffer(adamId);
    const keyUriStr = newStdStringFromArrayBuffer(uri);
    const keyFormatStr = newStdString("com.apple.streamingkeydelivery");
    const keyFormatVerStr = newStdString("1");
    const serverUriStr = newStdString("https://play.itunes.apple.com/WebObjects/MZPlay.woa/music/fps");
    const protocolTypeStr = newStdString("simplified");
    const fpsCertStr = newStdString(fairplayCert);

    const persistentKeyPtr = malloc(Process.pointerSize * 2);
    getPersistentKeyASM(
        sessionCtrlInstance,
        adamIdStr, keyUriStr,
        keyFormatStr, keyFormatVerStr,
        serverUriStr, protocolTypeStr,
        fpsCertStr, persistentKeyPtr
    );

    const decryptedKeyPtr = malloc(Process.pointerSize * 2);
    decryptContextASM(sessionCtrlInstance, persistentKeyPtr.readPointer(), decryptedKeyPtr);
    const kdContext = decryptedKeyPtr.readPointer().add(0x18).readPointer();

    if (kdContext.isNull()) {
        console.error("kdContext is null");
        return null;
    }

    kdContextMap.set(uriStr, kdContext);
    return kdContext;
}

async function handleDecryptionConnection(socket) {
    try {
        while (true) {
            const adamIdSize = (await socket.input.readAll(1)).unwrap().readU8();
            if (adamIdSize === 0) break;

            const adamId = await socket.input.readAll(adamIdSize);
            const uriSize = (await socket.input.readAll(1)).unwrap().readU8();
            const uri = await socket.input.readAll(uriSize);

            const kdContext = getKdContext(adamId, uri);
            if (!kdContext) {
                console.error("Failed to get kdContext");
                break;
            }


            while (true) {
                const sizeBuffer = await socket.input.readAll(4);
                if (sizeBuffer.byteLength === 0) break;
                const size = sizeBuffer.unwrap().readU32();
                if (size === 0) break;

                const sample = await socket.input.readAll(size);
                const sampleUnwrapped = sample.unwrap();
                decryptSample(kdContext.readPointer(), 5, sampleUnwrapped, sampleUnwrapped, sample.byteLength);
                await socket.output.writeAll(sample);
            }
        }
    } catch (e) {
        console.error("Connection handling error:", e);
        console.error(e.stack);
    } finally {
        await socket.close();
    }
}

global.getM3U8fromDownload = function(adamID) {
    var C8717f
    Java.choose("of.f", {
        onMatch: function (x) {
            C8717f = x
        },
        onComplete: function (x) {}
    });
    var response = C8717f.q(0, "", adamID, false)
    if (response.get().getError().get() == null){
        var item = response.get().getItems().get(0)
        var assets = item.get().getAssets()
        var size = assets.size()
        return assets.get(size - 1).get().getURL()
    } else {
        return response.get().getError().get().errorCode()
    }
};

const stringToByteArray = str => {
    const byteArray = [];
    for (let i = 0; i < str.length; ++i) {
        byteArray.push(str.charCodeAt(i));
    }
    return byteArray;
};

global.getM3U8 = function(adamID) {
    Java.use("com.apple.android.music.common.MainContentActivity");
    var SVPlaybackLeaseManagerProxy;
    Java.choose("com.apple.android.music.playback.SVPlaybackLeaseManagerProxy", {
        onMatch: function (x) {
            SVPlaybackLeaseManagerProxy = x
        },
        onComplete: function (x) {}
    });
    var HLSParam = Java.array('java.lang.String', ["HLS"])
    var MediaAssetInfo = SVPlaybackLeaseManagerProxy.requestAsset(parseInt(adamID), null, HLSParam, false)
    if (MediaAssetInfo === null) {
        return -1
    }
    return MediaAssetInfo.getDownloadUrl()
};

function performJavaOperations(adamID) {
    return new Promise((resolve, reject) => {
        Java.performNow(function () {
            const url = getM3U8(adamID);
            if (url === -1) {
                const url = getM3U8fromDownload(adamID);
                resolve(url);
            } else {
                resolve(url);
            }
        });
    });
}


async function handleM3U8Connection(s) {
    console.log("New M3U8 connection!");
    try {    
        const adamSize = (await s.input.readAll(1)).unwrap().readU8();
        if (adamSize !== 0) {
            const adam = await s.input.readAll(adamSize);
            const byteArray = new Uint8Array(adam);
            let adamID = "";
            for (let i = 0; i < byteArray.length; i++) {
                adamID += String.fromCharCode(byteArray[i]);
            }
            console.log("adamID:", adamID);
            let m3u8Url;
            performJavaOperations(adamID)
                .then(async (url) => {
                    m3u8Url = url;
                    console.log("M3U8 URL: ", m3u8Url);
                    const m3u8Array = stringToByteArray(m3u8Url + "\n");
                    // console.log("M3U8 ARRAY:", m3u8Array);
                    await s.output.writeAll(m3u8Array);
                })
                .catch((error) => {
                    console.error("Error performing Java operations:", error);
                });
        }
    } catch (err) {
        console.error("Error handling M3U8 connection:", err);
    }
    await s.close();
}

function initializeFridaFunctions(androidappmusic) {
    // Utility functions
    global.malloc = new NativeFunction(
        Module.findExportByName(null, "_Znwm"),
        'pointer',
        ['ulong']
    );
    global.dealloc = new NativeFunction(
        Module.findExportByName(null, "_ZdlPv"),
        'void',
        ['pointer']
    );

    // Apple Music specific functions
    global.sessionCtrlInstance = new NativeFunction(
        androidappmusic.getExportByName("_ZN21SVFootHillSessionCtrl8instanceEv"),
        'pointer',
        []
    )();


    global.getPersistentKeyAddr = androidappmusic.getExportByName("_ZN21SVFootHillSessionCtrl16getPersistentKeyERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_S8_S8_S8_S8_S8_");
    global.decryptContextAddr = androidappmusic.getExportByName("_ZN21SVFootHillSessionCtrl14decryptContextERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEERKN11SVDecryptor15SVDecryptorTypeERKb");

    global.decryptSample = new NativeFunction(
        androidappmusic.getExportByName("NfcRKVnxuKZy04KWbdFu71Ou"),
        'ulong',
        ['pointer', 'uint', 'pointer', 'pointer', 'size_t']
    );
}

function startListeners() {
    Socket.listen({ family: "ipv4", port: 20020 })
        .then(async (listener) => {
            while (true) {
                await handleM3U8Connection(await listener.accept());
            }
        })
        .catch(console.error);

    Socket.listen({ family: "ipv4", port: 10020 })
        .then(async (listener) => {
            while (true) {
                await handleDecryptionConnection(await listener.accept());
            }
        })
        .catch(console.error);
}

function waitForModule() {
    const moduleName = "libandroidappmusic.so";
    try {
        const androidappmusic = Process.getModuleByName(moduleName);
        initializeFridaFunctions(androidappmusic);
        startListeners();
        console.log(`Module ${moduleName} loaded successfully and listeners started`);
    } catch (e) {
        console.log(`Module ${moduleName} not loaded yet, waiting...`);
        setTimeout(waitForModule, 1000);
    }
}

waitForModule();