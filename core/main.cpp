#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <list>
#include <map>
#include <sstream>
#include <cmath>
#include <string>
#include <list>

#include <usb/UsbManager.h>
#include <X11/Xlib.h>
#include <apps/Cube/CubeApp.h>
#include <GLFW/glfw3.h>
#include <GL/glew.h>
#include <Server.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include "LightElement.h"
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Random.hpp"
#include <ansi_colors.hpp>
#include "core/arcball.h"
#include "core/commands/cmd.hpp"
#include "renderer/opengl/Axis.hpp"
#include "StringUtil.hpp"
#include "Background.h"
#include "Help.h"
#include "EventHandler.hpp"
#include "FontRenderer.h"
#include <Widget.h>
#include <EventGlfw.hpp>

int SCREEN_WIDTH = 200;
int SCREEN_HEIGHT = 200;

BackgroundStars background;
GLFWwindow* currentWindow;

const vec eye( 0.0f, 0.0f, -20.0f );
const vec centre( 0.0f, 0.0f, 0.0f );
const vec up( 0.0f, 1.0f, 0.0f );
const float SPHERE_RADIUS = 5.0f;

using namespace std;
using namespace hwidgets;

string lastState = "";

bool redisplayAsked = false;
bool backward = true;

// Lighting
LightElement ambientColor(1.0f, 1.0f, 1.0f, 0.8f, false);
LightElement specular(1.0f, 1.0f, 1.0f, 1.0f, true);
LightElement material(0,0,0,0,true);
LightElement shininess(10.0, 0,0,0,true);
Light light(5, 5, 3, 1, false);

long lastUpdateWidget = 0;

struct thr_info
{
	thread* thr;
	string cmd;
};

list<thr_info*> exec_list;

bool buttonRotate = false;
bool buttonTranslate = false;
float factor_translate = 0.1;
float translateX = 0;
float translateY = 0;
float translateZ = 0;
int translateStartX = 0;
int translateStartY = 0;

using namespace std;
GLint cube_server;
Axis axis;
Server* server;
GLuint program;
list<string> cmdQueue;
glm::mat4 orient;
float _anglex = 0;
float _angley = 0;
float animx = 0;
float animy = 0;
float i;
int k = 0;
const float BOX_SIZE = 2.0f;
GLuint white_textureId = 0;
GLuint red_textureId;
GLuint blue_textureId;
GLuint green_textureId;
GLuint yellow_textureId;
GLuint orange_textureId;
//Makes the image into a texture, and returns the id of the texture
bool resetTimer = true;

const float scale_org = 2.0;
float scale = scale_org;

const float hudr_org = 1.0;
const float hudv_org = 1.0;
const float hudb_org = 0.0;
bool hud = true;
float hudr = hudr_org;
float hudv = hudv_org;
float hudb = hudb_org;

map<string, string> macros;

CubeApp* getCube()
{
	return (CubeApp*) ApplicationBuilder::getInstance("cube");
}

#define cube getCube()

template <class T>
string toString(const T& t)
{
	std::stringstream ss;
	ss << t;
	return ss.str();
}

void trim(string& s, bool bWithCr = true)
{
	while (s.length() > 0 && (s[0] == ' ' || (bWithCr && (s[0] == 13 || s[0] == 10))))
		s.erase(0, 1);
	while (s.length() > 0 && (s[s.length() - 1] == ' ' || (bWithCr && (s[s.length() - 1] == 13 || s[s.length() - 1] == 10))))
		s.erase(s.length() - 1, 1);
}

void exec(string& cmd)
{
	trim(cmd);
	if (cmd.length())
	{
		try
		{
			bool bBackGround = cmd[cmd.length() - 1] == '&';
			if (bBackGround) cmd.erase(cmd.length() - 1, 1);
			thr_info* t = new thr_info();
			t->cmd = cmd;
			t->thr = new thread(
								[cmd]()
								  {
									system(cmd.c_str());
								server->send("#EVENT exec end " + cmd);
								  });
			exec_list.push_back(t);
			if (!bBackGround)
			{
				t->thr->join();
			}
			else
				exec_list.push_back(t);
		}
		catch (std::exception &e)
		{
			cerr << "THREAD EXCEPTION : " << e.what() << endl;
		}
	}
}

bool run(const string& sFileName)
{
	bool ret = false;
	ifstream queue(sFileName);
	if (queue.is_open())
	{
		ret = true;
		while (queue.good())
		{
			string cmd;
			getline(queue, cmd);
			cmdQueue.push_back(cmd);
		}
		cmdQueue.push_back("echo END OF SCRIPT: " + sFileName);
	}
   else
   {
      cmdQueue.push_back("echo ERROR WITH SCRIPT: " + sFileName);
      cerr << "run: Unable to open [" << sFileName << "]" << endl;
   }
	return ret;
}

void reboot()
{
   ApplicationBuilder::destroyAll();
	macros.clear();
	Widget::clear();

	run("startup.cub");
	run("macros.cub");
}

string getWord(string& s, const string &sSeparators)
{
	return StringUtil::getWord(s, sSeparators);
}

void handleResize(GLFWwindow* window, int w, int h)
{
	SCREEN_WIDTH = w;
	SCREEN_HEIGHT = h;
	hwidgets::Widget::handleResize(w, h);
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0, (double) w / (double) h, 1.0, 200);
	/*gluLookAt(0.0f, 5.5f, 15.0f,
			0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f);*/
    gluLookAt(
        eye.x, eye.y, eye.z,
        centre.x, centre.y, centre.z,
        up.x, up.y, up.z );
	arcball_setzoom( SPHERE_RADIUS, eye, up );
	/* if (w <= h)
	   glOrtho(-2.0, 2.0, -2.0 * (GLfloat) h / (GLfloat) w,
	   2.0 * (GLfloat) h / (GLfloat) w, -1.0, 1.0);
	   else
	   glOrtho(-2.0 * (GLfloat) w / (GLfloat) h,
	   2.0 * (GLfloat) w / (GLfloat) h, -2.0, 2.0, -1.0, 1.0);
	 */
	/* set matrix mode to modelview */
	glMatrixMode(GL_MODELVIEW);
}

void initRendering()
{
	//	float lpos[] = { 50, 50, 50, 0 };
	glShadeModel(GL_SMOOTH);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_NORMALIZE);
	glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	auto glError = glGetError();
	cout << "GlError = " << glError << endl;
}

float getFloat(string& incoming)
{
	string f = getWord(incoming);

	if (f.length() > 0 && ((f[0] >= '0' && f[0] <= '9') || f[0] == '.' || f[0] == '-'))
	{
		return atof(f.c_str());
	}
	return 0.0;
}

const Color* getColor(string& incoming)
{
	string org(incoming);
	const Color* color = Color::factory(incoming);

	if (color == 0)
		server->send("#ERROR Unknown color [" + org + "]");

	return color;
}

void onclose()
{
	delete server;
	server = 0;
   for(thr_info* thread_info : exec_list)
      if (thread_info->thr->joinable())
         thread_info->thr->join();
}

void drawText(const char * message)
{
	/* raster pos sets the current raster position
	 * mapped via the modelview and projection matrices
	 */
	//glRasterPos2f((GLfloat)0, (GLfloat)-400);
	//glRasterPos2f(0,0);

	/*
	 * write using bitmap and stroke chars
	 */
	while (*message)
	{
		// TODO with queso	glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *message);
		//glutStrokeCharacter(GLUT_STROKE_ROMAN,*message);
		message++;
	}
}

void update(int value);
void drawScene();

void widgetUpdate(int value)
{
	lastUpdateWidget = 0;
	// drawScene();
}

void drawHud()
{
	if (hud)
	{
		const float g_rotate = 0; // rotation du hud (fonctionne mal a cause de mouseevent pas en adéquation)
		Color c(hudr, hudv, hudb);

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0.0, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0, -1.0, 10.0);
		glMatrixMode(GL_MODELVIEW);
		glTranslatef(translateX, translateY,translateZ);

		glDisable(GL_CULL_FACE);

		glClear(GL_DEPTH_BUFFER_BIT);
		c.render();

		//glScalef(0.001, 0.001, 0.001);
		glRotatef(g_rotate, 0, 0, 1.0);
		glTranslatef(0, 0, 1.0);

		glRasterPos2f(2, SCREEN_HEIGHT - 2);
		//glPushMatrix();
		ApplicationBuilder::renderHud();
		glPopMatrix();

		//glTranslatef(0, 0, -1.0);
	}
   glPushMatrix();
	glLoadIdentity();
	long redisplay = Widget::renderAll();
	if (redisplay)
	{
		if (lastUpdateWidget == 0 || (redisplay < lastUpdateWidget))
		{
			lastUpdateWidget = redisplay;
			// TODO glutTimerFunc(redisplay, widgetUpdate, 0);
		}
	}

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void drawScene(GLFWwindow* window)
{
	/*if (!server->running())
	  exit(1);
	 */
   glEnable(GL_DEPTH_TEST);
	glMateriali(GL_FRONT_AND_BACK,GL_SHININESS,10);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef(translateX, translateY, translateZ);
	if (light.render())
		redisplayAsked = true;

   arcball_rotate();
	axis.render();

	if (material)
	{
		glEnable(GL_COLOR_MATERIAL);
		if (shininess)
			glMaterialfv(GL_FRONT, GL_SHININESS, shininess.getFloatArray());
	}
	else
		glDisable(GL_COLOR_MATERIAL);

	if (specular)
	{
		redisplayAsked |= !specular.isReady();
		glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,specular.getFloatArray());
	}
	background.render();
//	glTranslatef( eye.x, eye.y, eye.z ); //glTranslatef(cubex, cubey, cubez);
	// git grrr

//	glMultMatrixf(glm::value_ptr(orient));

	glScalef(scale, scale, scale);
	//glRotatef(_anglex, 1.0, 0.0, 0.0);
	//glRotatef(_angley, 0.0, 1.0, 0.0);

	if (ambientColor)
	{
		redisplayAsked |= !ambientColor.isReady();
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientColor.getFloatArray());
	}

	// Tentative désespérée de réflection ...
#if REFLEXION
	//glUniformMatrix4fv(cube_server, 1, GL_FALSE, glm::value_ptr(orient));
	glPushMatrix();
	glScalef(1.0, -1.0, 1.0);
	redisplayAsked |= ApplicationBuilder::render(false);
	glPopMatrix();

		  glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.7, 0.0, 0.0, 0.40);  /* 40% dark red floor color */
	Decor::render(600, 0, 600, 30);
	glDisable(GL_BLEND);
#endif
	redisplayAsked |= ApplicationBuilder::render(false);

	if (false) {
		static GLUquadricObj *quadric = 0;
		Color::cyan.render();
		if (quadric==0)
		{
			quadric = gluNewQuadric();
			gluQuadricDrawStyle(quadric, GLU_FILL );
		}
		gluSphere( quadric , 2 , 36 , 18 );
	}

	resetTimer = false;
	drawHud();
	glfwSwapBuffers(window);
}

void calcTranslate(string& incoming, float& v)
{
	StringUtil::trim(incoming);
	if (incoming.length())
		v = StringUtil::getFloat(incoming);
}

void mouse_motion_new(Event &event, Event::Mouse mouse)
{
	Widget::mouseMotion(event);	// FIXME Widget has to register events

	if (buttonRotate)
	{
		int invert_y = (SCREEN_HEIGHT - mouse.y) - 1;
		arcball_move(mouse.x,invert_y);
	}
	if (buttonTranslate)
	{
		translateX += (translateStartX-mouse.x)*factor_translate;
		translateY += (translateStartY-mouse.y)*factor_translate;
		translateStartX = mouse.x;
		translateStartY = mouse.y;
	}
	// TODO glutPostRedisplay();
}

void mouse_button_click(Event &event, Event::Mouse &mouse)
{
	/* si on appuie sur le bouton gauche */
	if (mouse.buttons.left)
	{
		cout << "on " << endl;
		buttonRotate = true;
		int invert_y = (SCREEN_HEIGHT - mouse.y) - 1; // OpenGL viewport coordinates are Cartesian
		arcball_start(mouse.x,invert_y);
	}
	if (mouse.buttons.middle)
	{
		buttonTranslate = true;
		translateStartX = mouse.x;
		translateStartY = mouse.y;
	}
		GLint pix[20];
		glReadPixels(mouse.x/SCREEN_WIDTH, mouse.y/SCREEN_HEIGHT, 1, 1, GL_RED, GL_INT8_NV, pix);	// WAS GL_FLOAT

		if (mouse.buttons.wheel_up)
			scale /= 0.98;
		if (mouse.buttons.wheel_down)
			scale *= 0.98;
/*	if (Widget::mouseButton(button, state, x, y) == 0 && (button == 4 || button == 3))
 * */
}

void mouseEvent(Event& event)
{
	//cout << event << endl;
	if (event.type == Event::EVT_MOUSE_DOWN)
		mouse_button_click(event, event.mouse);
	else if (event.type == Event::EVT_MOUSE_MOVE)
		mouse_motion_new(event, event.mouse);
	else if (event.type == Event::EVT_MOUSE_UP)
	{
		buttonRotate = false;
		buttonTranslate = false;
	}
}


void update(int value)
{

	string msg = Widget::popMessage();
	if (msg.length())
	{
		if (msg.find(":") != string::npos && msg.find("EVENT mousedown") != string::npos)
		{
			string cmd(msg);
			StringUtil::getWord(cmd, ':');
			if (cmd.substr(0, 5) == "stack")
				cmdQueue.push_front(cmd);
			else
			{
				cmdQueue.push_back(cmd);
				// cout << "Pushing [" << cmd << "]" << endl;
			}
		}

		server->send(msg);
	}


	int redisplay = 0;
	if (animy != 0 || animx != 0)
	{
		_angley += animy;
		if (_angley > 360) _angley -= 360;
		if (_angley < 0) _angley += 360;
		_anglex += animx;
		if (_anglex > 360)_anglex -= 360;
		if (_anglex < 0) _anglex += 360;

		redisplay = 479;
	}

	if (server)
	{
		if (server->incoming())
		{
			static string last;
			string incoming = server->getIncoming();

			if (incoming[0] == '.')	// TODO fix redundancy below with row/last
				incoming = last;
			else
				last = incoming;
			cmdQueue.push_back(incoming);

		}

		if (cmdQueue.size())
		{
			static string last;

			redisplay = 499;
			string row = cmdQueue.front();
			string org_row = row;

			cmdQueue.pop_front();

			while (row[0] == '@')
				row.erase(0, 1);
			string org = row;

			if (row == "!")
				row = last;
			else
				last = row;

			string incoming = getWord(row, ";");
			trim(row);
			if (row.length())
				cmdQueue.push_front(row);

			Widget::replaceVars(incoming);
			// cout << "INCOMMING " << incoming << endl;
			string cmd = getWord(incoming);
			// cout << "CMD=[" << cmd << "] incoming=[" << incoming << "]" << endl;

			//if (StringUtil::match("[a-zA-Z]+[a-zA-Z0-9]*.[a-zA-Z]+[a-zA-Z0-9]+=", cmd))

			if (cmd.length() && (cmd[0]=='#' || cmd.substr(0,2)=="//"))
			{

			}
			else if (StringUtil::preg_match("^[a-zA-Z]+[a-zA-Z0-9_]*\\.[a-zA-Z]+[a-zA-Z0-9_]*", cmd, false))
			{
				string name=StringUtil::getWord(cmd, '.');
				Application* object=ApplicationBuilder::getInstance(name);
				if (object)
				{
					IRunnable::ExecResult ret = object->execute(server, cmd, incoming, org, cmdQueue);
					switch (ret)
					{
						case IRunnable::EXEC_OK:
							server->send("#OK "+name+'.'+cmd);
							break;
						case IRunnable::EXEC_UNKNOWN:
							server->send("#Unknown command "+name+'.'+cmd);
							break;
						case Application::EXEC_FAILED:
							server->send("#KO "+name+'.'+cmd);
							break;
						case IRunnable::EXEC_BUSY:
						{
							cmdQueue.push_front(org);
						}
					};
				}
				else
				{
					server->send("#KO Object "+name+" unknown.");
				}
				cout << Ansi::reset();
			}
			else if (cmd == "help")
			{
				Help help;
				server->send("");
				server->send("Commands :");
				help.add("anim x|y value");
				help.add("axis");
				help.add("event {event} {allow|filter}");
				help.add("exec file");
				help.add("flat [x y z [scale]]");
				help.add("hud [color]");
				help.add("is_made");
				help.add("new {object}");
				help.add("macro");
				help.add("quit");
            help.add("font_path");  // TODO, should be in font renderer class
				help.add("reboot");
				help.add("scale s");
				help.add("screen x y");
				help.add("widget");
				help.add("translate [[x|y|z] value]*");
				help.add("var var=value");
				help.add("light [on|off|x y z t]");

				ApplicationBuilder::help(help);

				int count=0;
				// TODO modify widget with new help style
				Widget::help(incoming);

            for (const auto& help_item : help.get())
            {
					if (incoming.length()==0 || help_item.find(incoming)!=string::npos)
					{
						count++;
						server->send(help_item);
					}
            }
				if (count==0)
					server->send("No other help on '"+incoming+"'");

				server->send("");
			}
			else if (cmd == "var")
			{
				Widget::setVar(incoming);
			}
			else if (cmd == "translate")
			{
				while(incoming.length())
				{
					string what=StringUtil::getWord(incoming);
					if (what=="x")
						calcTranslate(incoming, translateX);
					else if (what=="y")
						calcTranslate(incoming, translateY);
					else if (what=="z")
						calcTranslate(incoming, translateZ);
					else
						server->send("x,y or z expected instead of "+what);
				}
			}
			else if (cmd == "scale")
			{
				scale = getFloat(incoming);
				server->send("#OK scale " + toString(scale));
			}
			else if (cmd == "hud")
			{
				if (incoming.length())
				{
					if (incoming == "reset")
					{
						hudr = hudr_org;
						hudv = hudv_org;
						hudb = hudb_org;
					}
					else
					{
						hudr = getFloat(incoming);
						if (incoming.length()) hudv = getFloat(incoming);
						if (incoming.length()) hudb = getFloat(incoming);
					}
					hud = true;
				}
				else
				{
					hud = !hud;
				}
				stringstream buf;
				buf << "#OK hud (" << (hud ? "ON" : "OFF") << ") (" << hudr << ", " << hudv << ", " << hudb << ")";
				server->send(buf.str());
			}
			else if (cmd == "axis")
			{
				axis.toggle();
				server->send(string("#OK axis RGB=XYZ ")+(axis.isActive() ? "ON" : "OFF"));
			}
			else if (cmd == "event")
			{
				string op = StringUtil::getWord(incoming);
				if (incoming.length() && (op == "allow" || op == "filter"))
					Widget::filtersOp(incoming, op == "filter");
				else if (op == "sim")
				{
					string evt = StringUtil::getWord(incoming);
					Widget::pushEvent(evt, incoming);
				}
				else
					server->send("#KO event, unknown operation : " + op);
			}
			else if (cmd == "macro")
			{
				string what = getWord(incoming);
				string args = "";

				map<string, string>* container = &macros;
				if (what == "exec")
				{
					what = getWord(incoming);
					args = ' ' + incoming;
					incoming = "";
				}

				if (what == "help")
				{
					server->send("  " + cmd + " [noreset] name        run the " + cmd + " name");
					server->send("  " + cmd + " name new_def          define the " + cmd + " name");
					server->send("  " + cmd + " delete name           delete the " + cmd + " name");
					server->send("  " + cmd + " load [file]           load a " + cmd + " file (default is " + cmd + "s.cub)");
					server->send("  " + cmd + " save [file]           save a " + cmd + " file");
				}
				else if (what == "save")
				{
					ofstream f(cmd + "s.cub");
					for (auto it = container->begin(); it != container->end(); it++)
						f << cmd << " " << it->first << " " << it->second << endl;
				}
				else if (what == "load")
				{
					run(cmd + "s.cub");
				}
				else if (what == "list")
				{
					string list;
					for (auto it = container->begin(); it != container->end(); it++)
						list += it->first + ' ';
					server->send("#OK " + cmd + " list=" + list);
				}
				else if (what == "delete")
				{
					auto it = container->find(incoming);
					if (it != container->end())
					{
						container->erase(it);
						cmdQueue.push_front('@' + cmd + " save");
						server->send("#OK " + cmd + " delete: '" + incoming + "'");
						if (cmd == "macro") cmdQueue.push_front("@macro save");
					}
					else
						server->send("#KO " + cmd + " delete: '" + incoming + "' not found");
				}
				else if (incoming.size() && what != "noreset")
				{
					string moves;
					if (incoming == "learn")
					{
						if (cube) moves = cube->learned();
					}
					else
						moves = incoming;
					(*container)[what] = moves;
					cmdQueue.push_front('@' + cmd + " save");
					server->send("#OK " + cmd + " " + what + " is defined");
				}
				else
				{
					auto it = container->find(what);
					if (it != container->end())
					{
						cmdQueue.push_front("@send #OK " + cmd);
						if (cmd == "pattern")
						{
							cmdQueue.push_front("@move " + it->second);
							if (incoming != "noreset") cmdQueue.push_front("@reset");
						}
						else
							cmdQueue.push_front('@' + it->second + args);
					}
					else
						server->send("#KO what (try " + cmd + " help)");
				}
			}
			else if (cmd == "echo")
			{
				cout << "# " << incoming << endl;
				server->send("# " + incoming);
			}
			else if (cmd == "exec")
			{
				exec(incoming);
			}
			else if (cmd == "light")
				server->send(light.read(cmd, incoming));
			else if (cmd == "ambient")
				server->send(ambientColor.read(cmd, incoming));
			else if (cmd == "material")
				server->send(material.read(cmd, incoming));
			else if (cmd == "specular")
				server->send(specular.read(cmd, incoming));
			else if (cmd == "shininess")
				server->send(shininess.read(cmd, incoming));
			else if (cmd == "anglex")
				_anglex=StringUtil::getFloat(incoming);
			else if (cmd == "angley")
				_angley = StringUtil::getFloat(incoming);
			else if (cmd == "send")
			{
				// cout << "SEND " << incoming << endl;
				server->send(incoming);
			}
			else if (cmd == "anim")
			{
				string a = getWord(incoming);
				if (a == "x")
					animx = getFloat(incoming);
				if (a == "y")
					animy = getFloat(incoming);
			}
         else if (cmd == "font_path")
         {
            FontRenderer::setFontPath(incoming);
         }
			else if (cmd == "quit")
			{
				cout << "QUIT !" << endl;
				server->stop();
            exit(1);
			}
			else if (cmd == "reboot")
			{
				reboot();
			}
			else if (cmd == "reset")
			{
				orient = glm::mat4();
				server->send("#OK");
				_anglex = 0;
				_angley = 0;
			}
			else if (cmd == "run")
			{
				run(incoming);
			}
			else if (cmd == "centers")
			{
				cmdQueue.push_back("find white");
				cmdQueue.push_back("find yellow");
				cmdQueue.push_back("find blue");
				cmdQueue.push_back("find orange");
				cmdQueue.push_back("find green");
				cmdQueue.push_back("find red");
			}
			else if (ApplicationBuilder::execute(server, cmd, incoming, org, cmdQueue) != IRunnable::EXEC_UNKNOWN)
			{
				cerr << "Ok// c ObjectBuilder.execute " << cmd << endl;
			}
			else if (Widget::onCommand(org_row))
				goto horrible; // @FIXME PTDR, un GOTO dans un code c++ Mouarf
			else if (macros.find(cmd) != macros.end())
			{
				cmdQueue.push_front("@macro exec " + cmd + " " + incoming);
			}
			else if (cmd.length())
			{
				if (core::cmd::execute(cmd, incoming, server))
				{
					cout << "COMMAND CORE::CMD OK" << endl;
				}
				else
					server->send("#KO Unknown command [" + cmd + "]");
			}
		}
	}

horrible:

	if (redisplay || redisplayAsked)
	{
		redisplayAsked = false;
		// TODO glutPostRedisplay();
	}
	else
		resetTimer = true;

	// TODO glutTimerFunc(10, update, 0);
}


/**
 * Get a normalized vector from the center of the virtual ball O to a
 * point P on the virtual ball surface, such that P is aligned on
 * screen's (X,Y) coordinates.  If (X,Y) is too far away from the
 * sphere, return the nearest point on the virtual ball surface.
 */
glm::vec3 get_arcball_vector(int x, int y) {
  glm::vec3 P = glm::vec3(1.0*x/SCREEN_WIDTH*2 - 1.0,
			  1.0*y/SCREEN_HEIGHT*2 - 1.0,
			  0);
  P.y = -P.y;
  float OP_squared = P.x * P.x + P.y * P.y;
  if (OP_squared <= 1*1)
    P.z = sqrt(1*1 - OP_squared);  // Pythagore
  else
    P = glm::normalize(P);  // nearest point
  return P;
}


void glfw_error_callback(int error, const char* desc)
{
	cerr << "ERROR: [GLFW] " << error << "," << desc << endl;
}


int main(int argc, char** argv)
{
	XInitThreads();
	//cmdQueue.push_back("rotate left 9000");
	//cmdQueue.push_back("rotate x");
	arcball_reset();

	if (!glfwInit())
	{
		cerr << "Unable to initialize glfw :-(" << endl;
		return 1;
	}
	Widget::init();
	Widget::setCmdQueue(&cmdQueue);
	reboot();

	long lPort;
	if (argc == 2)
		lPort = atol(argv[1]);
	else
		lPort = 3333;
	string sPort = toString(lPort);

	Widget::setVar("port=" + std::to_string(lPort));
	server = new Server(lPort);
	initRendering();

	glfwSetErrorCallback(glfw_error_callback);
	GLFWwindow* mainWindow = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, (string("CS ")+sPort).c_str(), nullptr, nullptr);
	currentWindow = mainWindow;
	glfwMakeContextCurrent(mainWindow);
	glewExperimental = GL_TRUE;

	GLenum err = glewInit();
   if (GLEW_OK != err)
	{
		cerr << "Failed to initialize GLEW " << glewGetErrorString(err) << endl;
		exit(1);
	}
	else
	{
		cout << "Glew init ok" << endl;
	}

	glfwSetWindowSizeCallback(mainWindow, handleResize);

	Event::init(EventGlfw::getInstance(mainWindow));
	EventHandler::connect(mouseEvent, Event::EVT_MOUSE_ALL);
	atexit(onclose);

	const double update_interval=0.025;
	double next_update=glfwGetTime() + update_interval;

	while(!glfwWindowShouldClose(mainWindow))
	{
		double curtime = glfwGetTime();
		if (curtime > next_update)
		{
			update(25);
			next_update += update_interval;
		}
		// TODO glutTimerFunc(25, update, 0);
		drawScene(mainWindow);
		glfwPollEvents();
      core::UsbManager::update();
	}

	return 0;

}

