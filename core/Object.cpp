/* 
 * File:   Object.cpp
 * Author: hsaturn
 * 
 * Created on 20 novembre 2015, 00:09
 */

#include "Object.h"
#include "StringUtil.hpp"
#include <stdlib.h>
#include <Widget.h>
#include <GL/glew.h>
#include <GL/glut.h>

Object::ExecResult Object::execute(Server* server, string cmd, string incoming, const string& org, CmdQueue& cmdQueue) {
	if (StringUtil::preg_match("^[a-zA-Z]+[a-zA-Z0-9_]*=", cmd, false)) {
		string name = StringUtil::getWord(cmd, '=');
		if (cmd.length() == 0) {
			auto it = mapVars.find(name);
			if (it != mapVars.end())
				mapVars.erase(it);
		} else
			mapVars [name] = cmd;
		return EXEC_OK;
	} else if (StringUtil::preg_match("^[a-zA-Z][a-zA-Z0-9_]*\\?", cmd)) {
		cmd.erase(cmd.length() - 1, 1);
		server->send("#OK " + getName() + '.' + cmd + "=" + getString(cmd));
		return EXEC_OK;
	} else if (cmd == "vars") {
		for (auto it : mapVars)
			server->send(getName() + '.' + it.first + '=' + it.second);
		server->send("Vars count : " + StringUtil::to_string(mapVars.size()));
		return EXEC_OK;
	} else
	{
		return _execute(server, cmd, incoming, org, cmdQueue);
	}
}

void Object::drawHudText(const string& txt) const {
	const char* message = txt.c_str();

	while (*message) {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *message);
		//glutStrokeCharacter(GLUT_STROKE_ROMAN,*message);
		message++;
	}
}

bool Object::render(bool resetTimer) {
	bool bRet = false;
	if (isVisible()) {
		glPushMatrix();
		glSetMatrix();
		bRet = _render(resetTimer);
		glPopMatrix();
	}
	return bRet;
}

void Object::glSetMatrix() {
	float scale = getFloat("scale", 1);
	glTranslatef(getFloat("dx"), getFloat("dy"), getFloat("dz"));
	glScalef(
		getFloat("sx", 1) * scale,
		getFloat("sy", 1) * scale,
		getFloat("sz", 1) * scale);
}

void Object::help(Help& help) {
	help.add("object.var=value   set var");
	help.add("object.var?        display var");
	help.add("object.dx/dy/dz    translate object");
	help.add("object.sx/sy/sz    scale object");
	help.add("object.vars        display vars of object");
	_help(help);
}

bool Object::isVisible() {
	auto it = mapVars.find("visible");
	if (it != mapVars.end())
		return atol(it->second.c_str());
	return true;
}

long Object::getLong(const string& name) const {
	return atol(getString(name).c_str());
}

float Object::getFloat(const string& name, float fdefault) const {
	auto it = mapVars.find(name);
	if (it != mapVars.end())
		return atof(it->second.c_str());
	return fdefault;
}

float Object::eatFloat(string& buf) const {
	string name = StringUtil::getWord(buf);
	auto it = mapVars.find(name);
	if (it != mapVars.end())
		return atof(it->second.c_str());
	return atof(name.c_str());
}

const string& Object::getString(const string& name) const {
	static const string empty("");
	auto it = mapVars.find(name);
	if (it != mapVars.end())
		return it->second;
	return empty;
}

int Object::getQuality() const {
	int quality = getLong("quality");
	if (quality == 0) {
		string v = hwidgets::Widget::getVar("quality");
		if (v.length()) {
			quality = atol(v.c_str());
		}
	}
	if (quality < 2) quality = 2;
	if (quality > 100) quality = 100;
	return quality;
}

bool Object::saveVars(ostream& out) const {
	out << "vars" << endl << "{" << endl;
	out << "  name=" << mname << endl;
	for (auto it : mapVars)
		out << "  " << it.first << '=' << it.second << endl;
	out << '}' << endl;
	return true;
}

bool Object::loadVars(istream& in) {
	string s;
	if (!in.good()) return false;
	in >> s;
	if (s == "vars") {
		mapVars.clear();
		while (in.good()) {
			getline(in, s);
			StringUtil::trim(s);
			string::size_type p = s.find('=');
			if (p != string::npos) {
				string name = s.substr(0, p);
				StringUtil::trim(name);
				s.erase(0, p + 1);
				StringUtil::trim(s);
				if (name.length()) {
					if (name=="name")
						mname = s;
					else
						mapVars[name] = s;
				} else
					cerr << "Empty name (for value " << s << ")" << endl;
			}
			if (s == "}")
				return true;
		}
		return false;
	} else
		return false;
}