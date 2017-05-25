//
//  main.m
//  ViroFBX
//
//  Created by Raj Advani on 4/25/17.
//  Copyright © 2017 Viro. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "VROFBXExporter.h"

int main(int argc, const char * argv[]) {
  @autoreleasepool {
      std::string heartFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/heart_beat_anim.fbx";
      std::string heartProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/heart.vrx";
      
      std::string minionFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/Hyperspace_Madness_Killamari_Minion.fbx";
      std::string minionProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/minion.vrx";
      
      std::string svenFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/Hyperspace_Madness_Sven.fbx";
      std::string svenProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/sven.vrx";

      std::string wormFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/worm_test.fbx";
      std::string wormProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/worm.vrx";
      
      std::string foxFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/fox.fbx";
      std::string foxProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/fox.vrx";

      VROFBXExporter *exporter = new VROFBXExporter();
      //exporter->debugPrint(heartFBX);
      exporter->exportFBX(heartFBX, heartProto);
      //exporter->exportFBX(minionFBX, minionProto);
      //exporter->exportFBX(svenFBX, svenProto);
      //exporter->exportFBX(wormFBX, wormProto);
      //exporter->exportFBX(foxFBX, foxProto);
  }
  return 0;
}
