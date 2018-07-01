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
	
	MadParameter* addParameter(ofJson parameterValues);
	std::map<std::string, MadParameter> parameterMap;
};
