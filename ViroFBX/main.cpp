#include <string>

#include "VROFBXExporter.h"
#include "VROImageExporter.h"
#include "VROLog.h"

void printUsage() {
    pinfo("Usage: ViroFBX [--compress-textures] [source FBX file] [destination VRX file]");
}

int main(int argc, const char *argv[]) {
    if (argc != 3 && argc != 4) {
        printUsage();
        return 1;
    }

    if (argc == 3) {
        VROFBXExporter *exporter = new VROFBXExporter();
        exporter->exportFBX(std::string(argv[1]), std::string(argv[2]), false);
    }
    else if (argc == 4) {
        std::string firstArg = argv[1];

        if (firstArg == "--compress-textures") {
            VROFBXExporter *exporter = new VROFBXExporter();
            exporter->exportFBX(std::string(argv[2]), std::string(argv[3]), true);
        }
        else {
            printUsage();
            return 1;
        }
    }

    return 0;
}
