//
//  main.m
//  ViroFBX
//
//  Created by Raj Advani on 4/25/17.
//  Copyright © 2017 Viro. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "VROFBXExporter.h"
#import "VROLog.h"

const bool kTestMode = NO;

void printUsage() {
    pinfo("Usage: ViroFBX [source FBX file] [destination VRX file]");
}

int main(int argc, const char * argv[]) {
  @autoreleasepool {
      if (kTestMode) {
          std::string heartFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/heart_beat_noskel.fbx";
          std::string heartProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/heart.vrx";
          
          std::string alienFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/aliengirl_mmastance_v2.fbx";
          std::string alienProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/aliengirl.vrx";
          
          std::string porscheFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/porsche911.fbx";
          std::string porscheProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/porsche911.vrx";
          
          std::string heartSkelFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/heart_beat_skel.fbx";
          std::string heartSkelProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/heart.vrx";
          
          std::string bballFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/bball_anim.fbx";
          std::string bballProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/bball.vrx";
          
          std::string minionFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/Hyperspace_Madness_Killamari_Minion.fbx";
          std::string minionProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/minion.vrx";
          
          std::string svenFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/Hyperspace_Madness_Sven.fbx";
          std::string svenProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/sven.vrx";
          
          std::string wormFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/worm_test.fbx";
          std::string wormProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/worm.vrx";
          
          std::string foxFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/fox.fbx";
          std::string foxProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/fox.vrx";
          
          std::string gorillaFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/gorilla.fbx";
          std::string gorillaProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/gorilla.vrx";
          
          VROFBXExporter *exporter = new VROFBXExporter();
          //exporter->debugPrint(heartFBX);
          
          exporter->exportFBX(alienFBX, alienProto);

          /*
          exporter->exportFBX(porscheFBX, porscheProto);
          exporter->exportFBX(bballFBX, bballProto);
          exporter->exportFBX(heartSkelFBX, heartSkelProto);
          exporter->exportFBX(minionFBX, minionProto);
          exporter->exportFBX(svenFBX, svenProto);
          exporter->exportFBX(wormFBX, wormProto);
          exporter->exportFBX(foxFBX, foxProto);
          exporter->exportFBX(gorillaFBX, gorillaProto);
           */
      }
      else {
          if (argc != 3) {
              printUsage();
              return 1;
          }
          
          VROFBXExporter *exporter = new VROFBXExporter();
          exporter->exportFBX(std::string(argv[1]), std::string(argv[2]));
          
          return 0;
      }
  }
  return 0;
}
