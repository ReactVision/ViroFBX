//
//  main.m
//  ViroFBX
//
//  Created by Raj Advani on 4/25/17.
//  Copyright © 2017 Viro. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "VROFBXLoader.h"

int main(int argc, const char * argv[]) {
  @autoreleasepool {
      VROFBXLoader *loader = new VROFBXLoader("/Users/radvani/Source/ViroFBX/macos/ViroFBX/heart_beat.fbx");
      loader->debugPrint();
      loader->exportFBX("/Users/radvani/Desktop/test.proto");
      
  }
  return 0;
}
