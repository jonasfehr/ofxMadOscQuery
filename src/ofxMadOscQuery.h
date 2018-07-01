//
//  MadOscQuery
//
//  Created by Jonas Fehr on 25/12/2017.
//  receiving OscQuery messages to generate External Control system
//  https://github.com/mrRay/OSCQueryProposal for more information


#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ofxOsc.h"
#include "ofxOscParameterSync.h"

#include "MadParameterPage.hpp"
#include "MadParameter.h"

#define DEBUG true

class ofxMadOscQuery{
public:
    
    string receiveAddress = "http://127.0.0.1:8010"; // default madmapper
    ofJson response;
    
    ofxOscSender oscSender;
    
   
    ofxPanel gui;

    string ip;
    int sendPort, receivePort;
    
    void setup(string ip, int sendPort, int receivePort);
    
    void oscSendToMadMapper(ofxOscMessage &m);
    
    
    ofJson receive();
    void setupMadParameterFromJson(MadParameter & newParameter, ofJson jsonParameterValues);

    void createOpacityPages(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json);
    void createSurfacePages(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json);
    void createMediaPages(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json);
    
    std::string getStatusString();
    
//    void parseMadSurfaces(ofJson response){
//        // create all surfaces
//        for(auto & s: response["CONTENTS"]["surfaces"]["CONTENTS"]){
//            if(s["DESCRIPTION"]=="selected" || s["DESCRIPTION"].is_null() ) continue;
//
//
//            // check if Group or Surface
//            if(s["CONTENTS"].size()>10){
//                // is group
//
////                if(c["FULL_PATH"].is_null()) continue;
//
//                Group group;
//                group.groupControl = parseSurfaceControls(s);
//
//                for(auto & gs: s["CONTENTS"]){
//                    if(gs["CONTENTS"].size()==9){
//                        Surface surface = parseSurfaceControls(gs);
////                        surfaces.insert(std::make_pair(surface.description, surface));
//                        group.surfaces.insert(std::make_pair(surface.description, surface));
//                    }
//                }
//                groups.insert(std::make_pair(group.groupControl.description, group));
//
//
//            } else {
//                // is Surface
//                Surface surface = parseSurfaceControls(s);
//                surfaces.insert(std::make_pair(surface.description, surface));
//
//
//            }
//            continue;
//        }
//    }
//
//    Surface parseSurfaceControls(ofJson s){
//
//        // create all surfaces
//
//        Surface surface;
//        // OscQueryEntry
//        surface.description = s["DESCRIPTION"];
//        surface.full_path = s["FULL_PATH"];
//        surface.access = s["ACCESS"];
//        surface.contents = s["CONTENTS"];
//
//        if(! s["CONTENTS"]["media_name"]["DESCRIPTION"].is_null())
//            surface.connectedMedia = s["CONTENTS"]["media_name"]["VALUE"];
//
//
//        vector<string> addressSegments = ofSplitString(s["FULL_PATH"].get<std::string>(), "/");
//        surface.parameterGroup.setName(addressSegments.end()[-1]);
//
//
//        // create surface controls
//        for(auto & c: s["CONTENTS"]){
//
//
//            if(c["DESCRIPTION"] == "Opacity") {
//                setupMadParameterFromJson(surface.opacity, c);
//                surface.parameterGroup.add(surface.opacity);
//            }else if(c["DESCRIPTION"] == "Color") {
//                for(auto & col: c["CONTENTS"]){
//                    if(col["DESCRIPTION"] == "Red"){
//                        setupMadParameterFromJson(surface.red, col);
//                        surface.parameterGroup.add(surface.red);
//                    }
//                    if(col["DESCRIPTION"] == "Green"){
//                        setupMadParameterFromJson(surface.green, col);
//                        surface.parameterGroup.add(surface.green);
//                    }
//                    if(col["DESCRIPTION"] == "Blue"){
//                        setupMadParameterFromJson(surface.blue, col);
//                        surface.parameterGroup.add(surface.blue);
//                    }
//                }
//            }else{
//                continue;
//            }
//
//
//            //            surfaces.insert(std::make_pair(surface.description, surface));
//        }
//        return surface;
//    }
//
//
//    void parseMadMedias(ofJson response){
//        // create all surfaces
//        for(auto & j: response["CONTENTS"]["medias"]["CONTENTS"]){
//            if(j["DESCRIPTION"] != "selected"){
//
//                Media media;
//                // OscQueryEntry
//                media.description = j["DESCRIPTION"];
//                media.full_path = j["FULL_PATH"];
//                media.access = j["ACCESS"];
//                media.contents = j["CONTENTS"];
//
//                // create surface controls
//                for(auto & p : j["CONTENTS"]["parameters"]["CONTENTS"]){
//                    if(p["ACCESS"]==3 && ( p["TYPE"] == "f" ) ){ //} || p["TYPE"] == "i" || p["TYPE"] == "b" ) ){
//                        //                    cout << p << endl;
//                        //                    cout << "-----" << endl;
//                        MadParameter newParameter;
//
//                        media.parameters.push_back( newParameter );
//                        setupMadParameterFromJson(media.parameters.back(), p);
//
//                    }
//                }
//                medias.insert(std::make_pair(media.description, media));
//            }
//        }
//    }
    

    

    
};



