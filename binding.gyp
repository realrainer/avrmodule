{
    "targets": [ {
        "target_name": "avrmodule",
        "sources": [ "addon.cc", "artspinput.cpp", "artspclient.cpp", "ahttpinput.cpp", "astreamdecode.cpp", "frameblocks.cpp", "tools.cpp" ],
        'include_dirs': [
          '/usr/include/ffmpeg',
          '/usr/include/liveMedia',
          '/usr/include/groupsock',
          '/usr/include/BasicUsageEnvironment',
          '/usr/include/UsageEnvironment',
          "<!(node -e \"require('nan')\")"
        ],
        'libraries': [
            '-lavfilter',
            '-lavformat',
            '-lliveMedia',
            '-lgroupsock',
            '-lBasicUsageEnvironment',
            '-lUsageEnvironment',
            '-lcurl'
        ]
    } ]
}
