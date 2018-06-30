//
//  Mixer.h
//  MadMapper_oscQUery
//
//  Created by Jonas Fehr on 06/04/2018.
//

#ifndef Mixer_h
#define Mixer_h

//class Channel{
//public:
//    ofParameter<float> fader;
//    ofParameter<bool> botMute;
//    ofParameter<bool> botSel;
//    ofParameter<bool> botRec;
//    
//};


class Mixer{
public:
    ofParameterGroup parameterGroup;
    
    void add(std::map<string,Surface> & surfaces_ref){
        parameterGroup.setName("surfaces");
        for( auto & s : surfaces_ref ){
            parameterGroup.add(s.second.parameterGroup);
        }
    }
    
    void add(std::map<string,Group> & Groupes_ref){
        parameterGroup.setName("groups");
        for( auto & g : Groupes_ref ){
            
            
            for( auto & s : g.second.surfaces ){
                g.second.groupControl.parameterGroup.add(s.second.parameterGroup);
            }
            
            parameterGroup.add(g.second.groupControl.parameterGroup);

        }
    }
    
};

#endif /* Mixer_h */
