/* 
 * File:   WidButton.cpp
 * Author: hsaturn
 * 
 * Created on 23 juin 2015, 23:32
 */

#include "Button.h"
#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut_std.h>

namespace hwidgets
{

	Button* Button::factory(string &infos)
	{
		Button* w = 0;
		if (infos.substr(0, 4) == "help")
		{
			Widget::pushMessage("button help:");
			Widget::pushMessage("  coords_topleft coords_width_height [left] string_text color_text");
			Color::factory(infos);
		}
		else
		{
			w = new Button();

			if (infos.substr(0, 4) == "left")
			{
				w->left = true;
				StringUtil::getWord(infos);
			}
			else
			{
				w->left = false;
			}

			w->text = StringUtil::getStringWord(infos, true);
			w->color = Color::factory(infos);
		}
		return w;
	}

	long Button::_render(long)
	{
		float x = rect()->x1() + width() / 2;
		float y = rect()->y2();


		string textr(text);
		replaceVars(textr);

		if (left)
			x = rect()->x1();
		else
			x -= textr.length()*4;

		y -= (height() - 13) / 2 + 2;

		if (color)
		{
			color->render();
		}
		else
			Color::dark_gray.render();

		glTranslatef(0.0, 0.0, 1.0);
		glRasterPos2f(x, y);

		for (auto it = textr.begin(); it != textr.end(); it++)
			glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *it);
		glTranslatef(0.0, 0.0, -1.0);
		return 0;
	}

	Button::Button() { }

	void Button::mouseClick(int button, int state, int x, int y)
	{
		if (button == GLUT_LEFT_BUTTON)
		{
			cout << "click " << button << "/" << GLUT_LEFT_BUTTON << endl;
			string event = "mouseup";
			if (state == GLUT_DOWN)
				event="mousedown";
			pushEvent(event, getName() + ' ' + StringUtil::to_string(button) + ' ' + StringUtil::to_string(state) + ' ' + getData());
		}
	}
}
