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
    
    ofxMadOscQuery();
    ~ofxMadOscQuery();
    
    
    string receiveAddress = "http://127.0.0.1:8010"; // default madmapper
    ofJson response;
    
    ofxOscSender oscSender;
    ofxOscReceiver oscReceiver;

    ofxPanel gui;

    string ip;
    
    string lastSelectedMedia;
//    string lastSelectedSurface;

    int sendPort, receivePort;
    
    void setup(string ip, int sendPort, int receivePort);
    
    void oscSendToMadMapper(ofxOscMessage &m);
    
    void oscReceiveMessages();

    ofJson receive();
    void setupMadParameterFromJson(MadParameter & newParameter, ofJson jsonParameterValues);

    void createOpacityPages(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json);
    void createSurfacePages(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json);
    void createMediaPages(std::map<string, MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json);
	void createCustomPage(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, std::string fileName);
    std::string getStatusString();
	
	MadParameter* addParameter(ofJson parameterValues);
	MadParameter* addParameter(ofJson parameterValues, std::string name);
	std::map<std::string, MadParameter> parameterMap;
};
