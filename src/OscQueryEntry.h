//
//  OscQueryEntry.h
//  MadMapper_oscQUery
//
//  Created by Jonas Fehr on 06/04/2018.
//

#ifndef OscQueryEntry_h
#define OscQueryEntry_h

#include "MadParameter.h"
#include "ofxOsc.h"

class OscQueryEntry{
public:
    int access;
    string full_path;
    ofJson contents;
    string description;
};



class Surface : public OscQueryEntry{
public:
    string connectedMedia = "";
    
    ofParameterGroup parameterGroup;
    MadParameter opacity;
    MadParameter red;
    MadParameter green;
    MadParameter blue;
    
//    vector<MadParameter> parameters;
};

class Group{
public:    
    Surface groupControl;
    std::map<string,Surface> surfaces;
};

class Media : public OscQueryEntry{
public:
    vector<MadParameter> parameters;
};

#endif /* OscQueryEntry_h */
