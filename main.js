/*! \file main.js
 *  \brief File transfer with WebRTC. Signaling is done through sound
 *  \author Georgi Gerganov
 */

var kOfferNumCandidates         = 5;
var kOfferName                  = '-';
var kOfferUsername              = '-';
var kOfferSessionId             = '1337';
var kOfferSessionVersion        = '0';
var kOfferLocalhost             = '0.0.0.0';
var kOfferBundle                = 'sdparta_0';
var kOfferPort                  = '5400';
var kOfferCandidateFoundation   = '0';
var kOfferCandidateComponent    = '1';
var kOfferCandidatePriority     = '2122252543';
var kOfferUfrag                 = 'bc105aa9';
var kOfferPwd                   = '52f0a329e7fd93662f50828f617b408d';
var kOfferFingerprint           = '00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00';

var kAnswerNumCandidates        = 5;
var kAnswerName                 = '-';
var kAnswerUsername             = '-';
var kAnswerSessionId            = '1338';
var kAnswerSessionVersion       = '0';
var kAnswerLocalhost            = '0.0.0.0';
var kAnswerBundle               = 'sdparta_0';
var kAnswerPort                 = '5400';
var kAnswerCandidateFoundation  = '0';
var kAnswerCandidateComponent   = '1';
var kAnswerCandidatePriority    = '2122252543';
var kAnswerUfrag                = 'c417de3e';
var kAnswerPwd                  = '1aa0e1241c16687064c4fd31b8fc367a';
var kAnswerFingerprint          = '00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00';

function getOfferTemplate() {
    return "v=0\r\n" +
        "o="+kOfferUsername+" "+kOfferSessionId+" "+kOfferSessionVersion+" IN IP4 "+kOfferLocalhost+"\r\n" +
        "s="+kOfferName+"\r\n" +
        "t=0 0\r\n" +
        "a=sendrecv\r\n" +
        "a=fingerprint:sha-256 "+kOfferFingerprint+"\r\n" +
        "a=group:BUNDLE "+kOfferBundle+"\r\n" +
        "a=ice-options:trickle\r\n" +
        "a=msid-semantic:WMS *\r\n" +
        "m=application "+kOfferPort+" DTLS/SCTP 5000\r\n" +
        "c=IN IP4 "+kOfferLocalhost+"\r\n" +
        "a=candidate:0 "+kOfferCandidateComponent+" UDP "+kOfferCandidatePriority+" "+kOfferLocalhost+" "+kOfferPort+" typ host\r\n" +
        "a=candidate:1 "+kOfferCandidateComponent+" UDP "+kOfferCandidatePriority+" "+kOfferLocalhost+" "+kOfferPort+" typ host\r\n" +
        "a=candidate:2 "+kOfferCandidateComponent+" UDP "+kOfferCandidatePriority+" "+kOfferLocalhost+" "+kOfferPort+" typ host\r\n" +
        "a=candidate:3 "+kOfferCandidateComponent+" UDP "+kOfferCandidatePriority+" "+kOfferLocalhost+" "+kOfferPort+" typ host\r\n" +
        "a=candidate:4 "+kOfferCandidateComponent+" UDP "+kOfferCandidatePriority+" "+kOfferLocalhost+" "+kOfferPort+" typ host\r\n" +
        "a=sendrecv\r\n" +
        "a=end-of-candidates\r\n" +
        "a=ice-pwd:"+kOfferPwd+"\r\n" +
        "a=ice-ufrag:"+kOfferUfrag+"\r\n" +
        "a=mid:"+kOfferBundle+"\r\n" +
        "a=sctpmap:5000 webrtc-datachannel 256\r\n" +
        "a=setup:actpass\r\n" +
        "a=max-message-size:1073741823\r\n";
}

function getAnswerTemplate() {
    return "v=0\r\n" +
        "o="+kAnswerUsername+" "+kAnswerSessionId+" "+kAnswerSessionVersion+" IN IP4 "+kAnswerLocalhost+"\r\n" +
        "s="+kAnswerName+"\r\n" +
        "t=0 0\r\n" +
        "a=sendrecv\r\n" +
        "a=fingerprint:sha-256 "+kAnswerFingerprint+"\r\n" +
        "a=group:BUNDLE "+kAnswerBundle+"\r\n" +
        "a=ice-options:trickle\r\n" +
        "a=msid-semantic:WMS *\r\n" +
        "m=application "+kAnswerPort+" DTLS/SCTP 5000\r\n" +
        "c=IN IP4 "+kAnswerLocalhost+"\r\n" +
        "a=candidate:0 "+kAnswerCandidateComponent+" UDP "+kAnswerCandidatePriority+" "+kAnswerLocalhost+" "+kAnswerPort+" typ host\r\n" +
        "a=candidate:1 "+kAnswerCandidateComponent+" UDP "+kAnswerCandidatePriority+" "+kAnswerLocalhost+" "+kAnswerPort+" typ host\r\n" +
        "a=candidate:2 "+kAnswerCandidateComponent+" UDP "+kAnswerCandidatePriority+" "+kAnswerLocalhost+" "+kAnswerPort+" typ host\r\n" +
        "a=candidate:3 "+kAnswerCandidateComponent+" UDP "+kAnswerCandidatePriority+" "+kAnswerLocalhost+" "+kAnswerPort+" typ host\r\n" +
        "a=candidate:4 "+kAnswerCandidateComponent+" UDP "+kAnswerCandidatePriority+" "+kAnswerLocalhost+" "+kAnswerPort+" typ host\r\n" +
        "a=sendrecv\r\n" +
        "a=end-of-candidates\r\n" +
        "a=ice-pwd:"+kAnswerPwd+"\r\n" +
        "a=ice-ufrag:"+kAnswerUfrag+"\r\n" +
        "a=mid:"+kAnswerBundle+"\r\n" +
        "a=sctpmap:5000 webrtc-datachannel 256\r\n" +
        "a=setup:active\r\n" +
        "a=max-message-size:1073741823\r\n";
}

// Taken from: https://github.com/diafygi/webrtc-ips
// get the IP addresses associated with an account
function getIPs(callback){
    var ip_dups = {};

    //compatibility for firefox and chrome
    var RTCPeerConnection = window.RTCPeerConnection
        || window.mozRTCPeerConnection
        || window.webkitRTCPeerConnection;
    var useWebKit = !!window.webkitRTCPeerConnection;

    //bypass naive webrtc blocking using an iframe
    if(!RTCPeerConnection){
        //NOTE: you need to have an iframe in the page right above the script tag
        //
        //<iframe id="iframe" sandbox="allow-same-origin" style="display: none"></iframe>
        //<script>...getIPs called in here...
        //
        var win = iframe.contentWindow;
        RTCPeerConnection = win.RTCPeerConnection
            || win.mozRTCPeerConnection
            || win.webkitRTCPeerConnection;
        useWebKit = !!win.webkitRTCPeerConnection;
    }

    //construct a new RTCPeerConnection
    var pc = new RTCPeerConnection(null);

    function handleCandidate(candidate){
        //match just the IP address
        //console.log(candidate);
        var ip_regex = /([0-9]{1,3}(\.[0-9]{1,3}){3}|[a-f0-9]{1,4}(:[a-f0-9]{1,4}){7})/
        var regex_res = ip_regex.exec(candidate);
        if (regex_res == null) return;
        var ip_addr = ip_regex.exec(candidate)[1];

        //remove duplicates
        if(ip_dups[ip_addr] === undefined)
            callback(ip_addr);

        ip_dups[ip_addr] = true;
    }

    //listen for candidate events
    pc.onicecandidate = function(ice){

        //skip non-candidate events
        if(ice.candidate)
            handleCandidate(ice.candidate.candidate);
    };

    //create a bogus data channel
    pc.createDataChannel("");

    //create an offer sdp
    pc.createOffer(function(result){

        //trigger the stun server request
        pc.setLocalDescription(result, function(){}, function(){});

    }, function(){});

    //wait for a while to let everything done
    setTimeout(function(){
        //read candidate info from local description
        var lines = pc.localDescription.sdp.split('\n');

        lines.forEach(function(line){
            if(line.indexOf('a=candidate:') === 0)
                handleCandidate(line);
        });
    }, 1000);
}

function transmitRelevantData(sdp, dataType) {
    var res = parseSDP(sdp);
    var hashparts = null;
    if (typeof res.fingerprint === 'undefined') {
        hashparts = res.media[0].fingerprint.hash.split(":");
    } else {
        hashparts = res.fingerprint.hash.split(":");
    }

    var r = new Uint8Array(256);
    r[0] = dataType.charCodeAt(0);

    var ip = document.getElementById('available-networks').value;
    r[2] = parseInt(ip.split(".")[0]);
    r[3] = parseInt(ip.split(".")[1]);
    r[4] = parseInt(ip.split(".")[2]);
    r[5] = parseInt(ip.split(".")[3]);

    for (var m in res.media[0].candidates) {
        if (res.media[0].candidates[m].ip != ip) continue;
        //if (res.media[0].candidate[m].transport != "UDP") continue;
        var port = res.media[0].candidates[m].port;
        r[6] = parseInt(port & 0xFF00) >> 8
        r[7] = parseInt(port & 0x00FF);
        break;
    }

    for (var i = 0; i < 32; ++i) {
        r[8+i] = parseInt(hashparts[i], 16);
    }

    var credentials = "";
    credentials += res.media[0].iceUfrag;
    credentials += " ";
    credentials += res.media[0].icePwd;
    for (var i = 0; i < credentials.length; ++i) {
        r[40 + i] = credentials.charCodeAt(i);
    }

    r[1] = 40 + credentials.length;

    var buffer = Module._malloc(256);
    Module.writeArrayToMemory(r, buffer, 256);
    Module.cwrap('setText', 'number', ['number', 'buffer'])(r[1], buffer);
    Module._free(buffer);
}

//
// Web RTC, Common
//

var sdpConstraints = { optional: [{RtpDataChannels: true}] };

var oldRxData = null;
var lastSenderRequest = null
var lastSenderRequestSDP = null
var lastSenderRequestTimestamp = null
var lastReceiverAnswer = null
var lastReceiverAnswerSDP = null
var lastReceiverAnswerTimestamp = null

var bitrateDiv = document.querySelector('div#bitrate');
var fileInput = document.querySelector('input#fileInput');
var downloadAnchor = document.querySelector('a#download');
var sendProgress = document.querySelector('progress#sendProgress');
var receiveProgress = document.querySelector('progress#receiveProgress');
var statusMessage = document.querySelector('span#status');
var peerInfo = document.querySelector('a#peer-info');
var peerReceive = document.querySelector('a#peer-receive');

var receiveBuffer = [];
var receivedSize = 0;

var bytesPrev = 0;
var timestampPrev = 0;
var timestampStart;
var statsInterval = null;
var bitrateMax = 0;

//
// Web RTC, Sender
//

var senderPC;
var senderDC;

function onAddIceCandidateSuccess() {
    console.log('AddIceCandidate success.');
}

function onAddIceCandidateError(error) {
    console.log('Failed to add Ice Candidate: ' + error.toString());
}

function createOfferSDP() {
    lastReceiverAnswerSDP = null;

    senderDC = senderPC.createDataChannel("fileTransfer");
    senderDC.binaryType = 'arraybuffer';
    senderDC.onopen = onSendChannelStateChange;
    senderDC.onclose = onSendChannelStateChange;
    senderPC.createOffer().then(function(e) {
        var res = parseSDP(e.sdp);

        res.name                    = kOfferName;
        res.origin.username         = kOfferUsername;
        res.origin.sessionId        = kOfferSessionId;
        res.origin.sessionVersion   = kOfferSessionVersion;
        res.groups[0].mids          = kOfferBundle;
        res.media[0].mid            = kOfferBundle;
        res.media[0].connection.ip  = document.getElementById('available-networks').value;

        e.sdp = writeSDP(res);
        senderPC.setLocalDescription(e);
    });
};

function senderInit() {
    senderPC = new RTCPeerConnection(null);

    senderPC.oniceconnectionstatechange = function(e) {
        //var state = senderPC.iceConnectionState;
    };

    senderPC.onicecandidate = function(e) {
        if (e.candidate) return;
        // send offer using sound
        transmitRelevantData(senderPC.localDescription.sdp, "O");
    }

    createOfferSDP();
}

function senderSend() {
    if (lastReceiverAnswerSDP == null) return;
    var answerDesc = new RTCSessionDescription(JSON.parse(lastReceiverAnswerSDP));
    if (senderPC) {
        senderPC.setRemoteDescription(answerDesc);
    }
}

//
// Web RTC, Receiver
//

var receiverPC;
var receiverDC;
var firstTimeFail = false;

function updatePeerInfo() {
    if (typeof Module === 'undefined') return;
    var framesLeftToRecord = Module.cwrap('getFramesLeftToRecord', 'number', [])();
    var framesToRecord = Module.cwrap('getFramesToRecord', 'number', [])();
    var framesLeftToAnalyze = Module.cwrap('getFramesLeftToAnalyze', 'number', [])();
    var framesToAnalyze = Module.cwrap('getFramesToAnalyze', 'number', [])();

    if (framesToAnalyze > 0) {
        peerInfo.innerHTML=
            "Analyzing Rx data: <progress value=" + (framesToAnalyze - framesLeftToAnalyze) +
            " max=" + (framesToRecord) + "></progress>";
        peerReceive.innerHTML= "";
    } else if (framesLeftToRecord > Math.max(0, 0.05*framesToRecord)) {
        firstTimeFail = true;
        peerInfo.innerHTML=
            "Sound handshake in progress: <progress value=" + (framesToRecord - framesLeftToRecord) +
            " max=" + (framesToRecord) + "></progress>";
        peerReceive.innerHTML= "";
    } else if (framesToRecord > 0) {
        peerInfo.innerHTML= "Analyzing Rx data ...";
    } else if (framesToRecord == -1) {
        if (firstTimeFail) {
            playSound("/media/case-closed");
            firstTimeFail = false;
        }
        peerInfo.innerHTML= "<p style=\"color:red\">Failed to decode Rx data</p>";
    }
}

function parseRxData(brx) {
    var vals = Array();
    vals[0] = "";
    vals[0] += String(brx[2]) + ".";
    vals[0] += String(brx[3]) + ".";
    vals[0] += String(brx[4]) + ".";
    vals[0] += String(brx[5]);

    vals[1] = String(brx[6]*256 + brx[7]);

    vals[2] = "";
    for (var i = 0; i < 32; ++i) {
        if (brx[8+i] == 0) {
            vals[2] += '00';
        } else if (brx[8+i] < 16) {
            vals[2] += '0'+brx[8+i].toString(16).toUpperCase();
        } else {
            vals[2] += brx[8+i].toString(16).toUpperCase();
        }
        if (i < 31) vals[2] += ':';
    }

    var credentials = "";
    for (var i = 40; i < brx[1]; ++i) {
        credentials += String.fromCharCode(brx[i]);
    }
    vals[3] = credentials.split(" ")[0];
    vals[4] = credentials.split(" ")[1];

    return vals;
}

function checkRxForPeerData() {
    if (typeof Module === 'undefined') return;
    Module.cwrap('getText', 'number', ['buffer'])(bufferRx);
    var result = "";
    for(var i = 0; i < 82; ++i){
        result += (String.fromCharCode((Module.HEAPU8)[bufferRx + i]));
        brx[i] = (Module.HEAPU8)[bufferRx + i];
    }

    if (String.fromCharCode(brx[0]) == "O") {
        var lastSenderRequestTmp = brx;
        if (lastSenderRequestTmp == lastSenderRequest) return;

        console.log("Received Offer");
        lastSenderRequest = lastSenderRequestTmp;

        var vals = parseRxData(brx);
        var res = parseSDP(getOfferTemplate());

        res.origin.username             = kOfferUsername;
        res.origin.sessionId            = kOfferSessionId;
        res.media[0].iceUfrag           = vals[3];
        res.media[0].icePwd             = vals[4];
        res.media[0].connection.ip      = vals[0];
        res.media[0].port               = vals[1];
        if (typeof res.fingerprint === 'undefined') {
            res.media[0].fingerprint.hash   = vals[2];
        } else {
            res.fingerprint.hash   = vals[2];
        }
        if (typeof res.candidates === 'undefined') {
            for (var i = 0; i < kOfferNumCandidates; ++i) {
                res.media[0].candidates[i].ip       = vals[0];
                res.media[0].candidates[i].port     = vals[1];
                res.media[0].candidates[i].priority = kOfferCandidatePriority;
            }
        } else {
            for (var i = 0; i < kOfferNumCandidates; ++i) {
                res.candidates[i].ip       = vals[0];
                res.candidates[i].port     = vals[1];
                res.candidates[i].priority = kOfferCandidatePriority;
            }
        }

        //console.log(writeSDP(res));
        lastSenderRequestSDP = '{"type":"offer","sdp":'+JSON.stringify(writeSDP(res))+'}';
        peerInfo.innerHTML= "Receive file from " + vals[0] + " ?";
        peerReceive.innerHTML= "<button onClick=\"lockoutSubmit(this); receiverInit();\">Receive</button>";
        playSound("/media/open-ended");

        return;
    } else {
        lastSenderRequest = null;
    }

    if (String.fromCharCode(brx[0]) == "A") {
        var lastReceiverAnswerTmp = brx;
        if (lastReceiverAnswerTmp == lastReceiverAnswer) return;

        console.log("Received Answer");
        lastReceiverAnswer = lastReceiverAnswerTmp;

        var vals = parseRxData(brx);
        var res = parseSDP(getAnswerTemplate());

        res.origin.username             = kAnswerUsername;
        res.origin.sessionId            = kAnswerSessionId;
        res.media[0].iceUfrag           = vals[3];
        res.media[0].icePwd             = vals[4];
        res.media[0].connection.ip      = vals[0];
        res.media[0].port               = vals[1];
        if (typeof res.fingerprint === 'undefined') {
            res.media[0].fingerprint.hash   = vals[2];
        } else {
            res.fingerprint.hash   = vals[2];
        }
        if (typeof res.candidates === 'undefined') {
            for (var i = 0; i < kAnswerNumCandidates; ++i) {
                res.media[0].candidates[i].ip       = vals[0];
                res.media[0].candidates[i].port     = vals[1];
                res.media[0].candidates[i].priority = kAnswerCandidatePriority;
            }
        } else {
            for (var i = 0; i < kAnswerNumCandidates; ++i) {
                res.candidates[i].ip       = vals[0];
                res.candidates[i].port     = vals[1];
                res.candidates[i].priority = kAnswerCandidatePriority;
            }
        }

        lastReceiverAnswerSDP = '{"type":"answer","sdp":'+JSON.stringify(writeSDP(res))+'}';
        playSound("/media/open-ended");

        if (senderPC) {
            peerInfo.innerHTML= "Trying to connect with " + vals[0] + " ...";
            senderSend();
        } else {
            peerInfo.innerHTML= "Received answer not meant for us (" + vals[0] + ")";
        }

        return;
    } else {
        lastReceiverAnswer = null;
    }
}

function createAnswerSDP() {
    if (lastSenderRequestSDP == null) return;
    var offerDesc = new RTCSessionDescription(JSON.parse(lastSenderRequestSDP));
    receiverPC.setRemoteDescription(offerDesc,
        function() {
            receiverPC.createAnswer(
                function (e) {
                    var res = parseSDP(e.sdp);

                    res.name                    = kAnswerName;
                    res.origin.username         = kAnswerUsername;
                    res.origin.sessionId        = kAnswerSessionId;
                    res.origin.sessionVersion   = kAnswerSessionVersion;
                    res.media[0].mid            = kAnswerBundle;
                    res.media[0].connection.ip  = document.getElementById('available-networks').value;

                    e.sdp = writeSDP(res);
                    receiverPC.setLocalDescription(e);
                },
                function () { console.warn("Couldn't create offer") },
                sdpConstraints
            );
        }, function(e) {
            console.log("Could not set remote description. Reason: " + e);
        });
};

function receiverInit() {
    receiverPC = new RTCPeerConnection(null);

    receiverPC.ondatachannel = receiveChannelCallback;
    receiverPC.onicecandidate = function(e) {
        if (e.candidate) return;
        // send answer using sound
        transmitRelevantData(receiverPC.localDescription.sdp, "A");
    };
    receiverPC.oniceconnectionstatechange = function(e) {};

    createAnswerSDP();
}

//
// File sutff
//

function ab2str(buf) {
    return String.fromCharCode.apply(null, new Uint16Array(buf));
}

function str2ab(str) {
    var buf = new ArrayBuffer(str.length*2); // 2 bytes for each char
    var bufView = new Uint16Array(buf);
    for (var i=0, strLen=str.length; i<strLen; i++) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}

fileInput.addEventListener('change', handleFileInputChange, false);

function handleFileInputChange() {
    var file = fileInput.files[0];
    if (!file) {
        console.log('No file chosen');
    }
}

function sendData() {
    var file = fileInput.files[0];
    if (file == null) {
        peerInfo.innerHTML = "Connection established, but no file selected";
        return;
    }
    peerInfo.innerHTML = "Sending selected file to peer ...";
    console.log('File is ' + [file.name, file.size, file.type, file.lastModifiedDate ].join(' '));

    // Handle 0 size files.
    statusMessage.textContent = '';
    downloadAnchor.textContent = '';
    if (file.size === 0) {
        bitrateDiv.innerHTML = '';
        statusMessage.textContent = 'File is empty, please select a non-empty file';
        closeDataChannels();
        return;
    }
    senderDC.send(str2ab(file.name));
    senderDC.send(str2ab(String(file.size)));
    sendProgress.max = file.size;
    receiveProgress.max = file.size;
    var chunkSize = 16384;
    var sliceFile = function(offset) {
        var reader = new window.FileReader();
        reader.onload = (function() {
            return function(e) {
                if (senderDC.bufferedAmount > 4*1024*1024) {
                    window.setTimeout(sliceFile, 100, offset);
                } else {
                    senderDC.send(e.target.result);
                    if (file.size > offset + e.target.result.byteLength) {
                        window.setTimeout(sliceFile, 0, offset + chunkSize);
                    }
                    sendProgress.value = offset + e.target.result.byteLength;
                }
            };
        })(file);
        var slice = file.slice(offset, offset + chunkSize);
        reader.readAsArrayBuffer(slice);
    };
    sliceFile(0);
}

function closeDataChannels() {
    console.log('Closing data channels');
    if (senderDC) {
        senderDC.close();
        console.log('Closed data channel with label: ' + senderDC.label);
    }
    if (receiverDC) {
        receiverDC.close();
        console.log('Closed data channel with label: ' + receiverDC.label);
    }

    if (senderPC) senderPC.close();
    if (receiverPC) receiverPC.close();

    senderPC = null;
    receiverPC = null;
    console.log('Closed peer connections');

    // re-enable the file select
    fileInput.disabled = false;
}

function onSendChannelStateChange() {
    var readyState = senderDC.readyState;
    console.log('Send channel state is: ' + readyState);
    if (readyState === 'open') {
        sendData();
    }
}

function receiveChannelCallback(event) {
    console.log('Receive Channel Callback');
    receiverDC = event.channel;
    receiverDC.binaryType = 'arraybuffer';
    receiverDC.onmessage = onReceiveMessageCallback;
    receiverDC.onopen = onReceiveChannelStateChange;
    receiverDC.onclose = onReceiveChannelStateChange;

    receivedSize = 0;
    recvFileName = '';
    recvFileSize = 0;
    bitrateMax = 0;
    downloadAnchor.textContent = '';
    downloadAnchor.removeAttribute('download');
    if (downloadAnchor.href) {
        URL.revokeObjectURL(downloadAnchor.href);
        downloadAnchor.removeAttribute('href');
    }
}

var recvFileName = '';
var recvFileSize = 0;

function onReceiveMessageCallback(event) {
    //console.log('Received Message ' + event.data.byteLength);
    if (recvFileName == '') {
        recvFileName = ab2str(event.data);
        return;
    }
    if (recvFileSize == 0) {
        recvFileSize = parseInt(ab2str(event.data));
        peerInfo.innerHTML = "Receiving file '" + recvFileName + "' (" + recvFileSize + " bytes) ...";
        receiveProgress.max = recvFileSize;
        return;
    }

    receiveBuffer.push(event.data);
    receivedSize += event.data.byteLength;

    receiveProgress.value = receivedSize;

    // we are assuming that our signaling protocol told
    // about the expected file size (and name, hash, etc).
    //var file = fileInput.files[0];
    if (receivedSize === recvFileSize) {
        var received = new window.Blob(receiveBuffer);
        receiveBuffer = [];

        downloadAnchor.href = URL.createObjectURL(received);
        downloadAnchor.download = recvFileName;
        downloadAnchor.textContent = 'Click to download \'' + recvFileName + '\' (' + recvFileSize + ' bytes)';
        downloadAnchor.style.display = 'block';

        var bitrate = Math.round(8*receivedSize/((new Date()).getTime() - timestampStart));
        bitrateDiv.innerHTML = 'Average Bitrate: ' + bitrate + ' kbits/sec (max: ' + bitrateMax + ' kbits/sec)';

        if (statsInterval) {
            window.clearInterval(statsInterval);
            statsInterval = null;
        }

        closeDataChannels();
    }
}

function onReceiveChannelStateChange() {
    var readyState = receiverDC.readyState;
    console.log('Receive channel state is: ' + readyState);
    if (readyState === 'open') {
        timestampStart = (new Date()).getTime();
        timestampPrev = timestampStart;
        statsInterval = window.setInterval(displayStats, 500);
        window.setTimeout(displayStats, 100);
        window.setTimeout(displayStats, 300);
    }
}

// display bitrate statistics.
function displayStats() {
    var display = function(bitrate) {
        bitrateDiv.innerHTML = '<strong>Current Bitrate:</strong> ' +
            bitrate + ' kbits/sec';
    };

    if (receiverPC && receiverPC.iceConnectionState === 'connected') {
        // Firefox currently does not have data channel stats. See
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1136832
        // Instead, the bitrate is calculated based on the number of
        // bytes received.
        var bytesNow = receivedSize;
        var now = (new Date()).getTime();
        var bitrate = Math.round(8*(bytesNow - bytesPrev)/(now - timestampPrev));
        display(bitrate);
        timestampPrev = now;
        bytesPrev = bytesNow;
        if (bitrate > bitrateMax) {
            bitrateMax = bitrate;
        }
    }
}
