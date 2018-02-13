//
//  main.m
//  ViroFBX
//
//  Created by Raj Advani on 4/25/17.
//  Copyright © 2017 Viro. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "VROFBXExporter.h"
#import "VROImageExporter.h"
#import "VROLog.h"

const bool kTestMode = YES;

void printUsage() {
    pinfo("Usage: ViroFBX [--compress-textures] [source FBX file] [destination VRX file]");
}

int main(int argc, const char * argv[]) {
  @autoreleasepool {
      if (kTestMode) {
          std::string playaEXR = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/Playa_Sunrise.exr";
          std::string playaOut = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/sunrise.vhd";
          
          std::string woodenHDR = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/WoodenDoor_Ref.hdr";
          std::string woodenOut = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/wooden.vhd";
          
          std::string smileFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/emoji_smile_anim_a.fbx";
          std::string smileProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/emoji_smile_anim.vrx";
          
          std::string pumpkinFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/object_pumpkin_anim.fbx";
          std::string pumpkinProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/pumpkin.vrx";
          
          std::string cylinderFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/cylinder_pbr.fbx";
          std::string cylinderProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/cylinder_pbr.vrx";
          
          std::string dragonFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/dragao_2018.fbx";
          std::string dragonProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/dragon.vrx";
          
          std::string starFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/object_star_anim.fbx";
          std::string starProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/object_star_anim.vrx";
          
          std::string pugFBX = "/Users/radvani/Source/ViroFBX/macos/ViroFBX/dog_animated_v3.fbx";
          std::string pugProto = "/Users/radvani/Source/ViroRenderer/ios/ViroSample/pug.vrx";
          
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
          
          //VROImageExporter *imageExporter = new VROImageExporter();
          //imageExporter->exportHDR(woodenHDR, woodenOut, VROImageOutputFormat::RGB9E5);
          //imageExporter->exportEXR(playaEXR, playaOut, VROImageOutputFormat::RGB9E5);
          VROFBXExporter *exporter = new VROFBXExporter();
          //exporter->debugPrint(cylinderFBX);
          //exporter->debugPrint(pumpkinFBX);
          //exporter->exportFBX(pumpkinFBX, pumpkinProto, false);
          exporter->exportFBX(dragonFBX, dragonProto, false);
          //exporter->exportFBX(starFBX, starProto, false);
          //exporter->exportFBX(smileFBX, smileProto, false);
          //exporter->exportFBX(pugFBX, pugProto, false);
          /*
          exporter->exportFBX(alienFBX, alienProto);
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
  }
  return 0;
}
