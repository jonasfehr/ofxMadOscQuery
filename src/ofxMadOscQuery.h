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

class ofxMadOscQuery
{
public:
    ofxMadOscQuery();
    ~ofxMadOscQuery();

    string receiveAddress = "http://127.0.0.1:8010"; // default madmapper

    ofxOscSender oscSender;
    ofxOscReceiver oscReceiver;

    ofxPanel gui;

    string ip;

    //    string lastSelectedMedia;
    //    string lastSelectedSurface;

    int sendPort, receivePort;

    void setup(string ip, int sendPort, int receivePort);

    void oscSendToMadMapper(ofxOscMessage &m);

    void oscReceiveMessages(ofParameterGroup &syncGroup);

    ofJson receive();
    //    void createParameterMap(ofJson json);
    //    void getParameterList(ofJson json, vector<string> skipKeys);
    map<string, ofJson> getContentMap(ofJson json, string key, vector<string> skipKeys);
    //    void iterateContents(ofJson json);
    void iterateFind(ofJson json, string key, MadParameterPage *customPage, ofJson jsonSkipKeys);
    void iterateFind(ofJson &jsonReturn, ofJson json, string key, ofJson jsonSkipKeys);

    void createSubPages(std::list<MadParameterPage> &page, ofxMidiDevice *midiDevice, ofJson json);
    void setupPageFromJson(std::list<MadParameterPage> &pages, MadParameterPage &page, ofxMidiDevice *midiDevice, ofJson json, string keyType);

    void createCustomPage(std::list<MadParameterPage> &pages, ofxMidiDevice *midiDevice, ofJson json);
    void createCustomPages(ofxMidiDevice *midiDevice, ofJson jsonPages, ofJson madMapperJson);
    std::string getStatusString();
    bool matchesGroupWildcard(std::string paramName, std::string elementName);
    void getConnectedMediaName(string *mediaName, ofJson json, string key, ofJson jsonSkipKeys);

    MadParameter *createParameter(ofJson parameterValues);
    MadParameter *createParameter(ofJson parameterValues, std::string name);
    void addParameterToCustomPage(ofJson element, std::string type, MadParameterPage *customPage);
    std::map<std::string, MadParameter> parameterMap;

    void updateValues();

    ofEvent<string> mediaNameE;

    std::list<MadParameterPage> pages;
    std::list<MadParameterPage> subPages;
    std::list<MadParameterPage> mediaPages;
    ofJson madMapperJson;
};
