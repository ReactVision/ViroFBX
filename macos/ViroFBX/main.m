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
      VROFBXExporter *exporter = new VROFBXExporter("/Users/radvani/Source/ViroFBX/macos/ViroFBX/heart_beat_triangles.fbx");
      exporter->debugPrint();
      exporter->exportFBX("/Users/radvani/Source/ViroRenderer/ios/ViroSample/heart.proto");
      
  }
  return 0;
}
