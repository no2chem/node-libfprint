cmd_Release/fprint.node := ln -f "Release/obj.target/fprint.node" "Release/fprint.node" 2>/dev/null || (rm -rf "Release/fprint.node" && cp -af "Release/obj.target/fprint.node" "Release/fprint.node")
