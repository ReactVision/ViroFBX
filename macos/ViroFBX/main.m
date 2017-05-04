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
      std::string heartFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/heart_beat_triangles.fbx";
      std::string heartProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/heart.proto";
      
      std::string minionFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/Hyperspace_Madness_Killamari_Minion.fbx";
      std::string minionProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/minion.proto";
      
      std::string svenFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/Hyperspace_Madness_Sven.fbx";
      std::string svenProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/sven.proto";


      VROFBXExporter *exporter = new VROFBXExporter();
//      exporter->exportFBX(heartFBX, heartProto);
//      exporter->debugPrint(minionFBX);
      exporter->exportFBX(svenFBX, svenProto);
      
  }
  return 0;
}
