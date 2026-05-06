/*
 A page contains a list of MadParameters
 */
#pragma once

#include "MadParameter.h"
#include "ofxMidiDevice.h"

class MadParameterPage
{
public:
	ofParameterGroup linkedParamGroup;

	MadParameterPage(std::string name, ofxMidiDevice *midiDevice, int numParamVis = 13, bool isSubpage = false, bool isGroup = false);
	~MadParameterPage();

	void setValuesOnDevice(ofAbstractParameter &p);
	void addParameter(MadParameter *parameter);
	void setLowerBound(int lower);
	std::string getLatestParameterName();
	bool isEmpty();
	std::string getName();
	std::list<MadParameter *> *getParameters();
	const std::list<MadParameter *>* getParameters() const;
	void cycleForward();
	void cycleBackward();
	void linkDevice();
	void unlinkDevice();
	std::pair<int, int> getRange();
	bool isSubpage();
	bool isGroup();
	void setIsGroup(bool isGroup);

	std::list<MadParameterPage>::iterator parentPage;

private:
	bool bSubpage = false;
	bool bIsGroup = false;
	int numParamVis = 13;
	std::list<MadParameter *> parameters;
	std::string name = "";
	std::pair<int, int> range{0, 0};
	ofxMidiDevice *midiDevice = nullptr;
};
