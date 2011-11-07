#include "cinder/app/AppBasic.h"
#include "cinder/Rand.h"
#include "cinder/gl/gl.h"
#include "SoundEngine.h"

using namespace ci;
using namespace ci::app;

#include <list>
using namespace std;

const float frame_time = 1.0f / 60.0f;

class CLayer
{
public:
	static CLayer* CreateRandom()
	{
		int num_divs = Rand::randInt(2, 20);
		float rotate_rate = Rand::randFloat(-20, 20);
		float inner_rotate = Rand::randFloat(-2, 2);
		float radius = Rand::randFloat(0.1f, 2);
		float life_time = Rand::randFloat(1.5f, 3.0f);

		return new CLayer(num_divs, rotate_rate, inner_rotate, radius, life_time);
	}

	bool Update(float delta_time)
	{
		m_CurrLife -= delta_time;

		if(m_CurrLife > 0)
		{
			return true;
		}

		return false;
	}

	void draw(float time, float w, float h)
	{
		glPushMatrix();

		float life_percent = math<float>::clamp(m_CurrLife / m_TotalLife);
		float inv_life_percent = 1 - life_percent;
		float in_out = ((float)sin(inv_life_percent * M_PI * 2) + 1) * 0.5f;

		gl::rotate(time * m_MasterRotateRate);

		int num_divs = 10;
		for(int i=0; i<num_divs; i++)
		{
			float i_f = i/(float)num_divs;
			gl::color(hsvToRGB(Vec3f(i_f, 1, 1)));

			float sin_t = sin(time*1.3f);

			glPushMatrix();

			gl::rotate(i_f*360);
			
			gl::translate(Vec2f(0, w * 0.25f * m_Radius));

			float rect_w = 10;
			float rect_h = 10;

			float cube_size = 20;
			gl::rotate(Vec3f(time*18*m_InnerRotateRate, time*36*m_InnerRotateRate, 0));

			gl::scale(Vec3f(in_out, in_out, in_out));

			gl::drawCube(Vec3f::zero(), Vec3f(1,1,1) * (cube_size * (sin_t+2)));

			glPopMatrix();
		}

		glPopMatrix();
	}

private:
	CLayer(int num_divs, float rotate_rate, float inner_rotate, float radius, float life_time)
		:
	m_NumDivs(num_divs),
		m_MasterRotateRate(rotate_rate),
		m_InnerRotateRate(inner_rotate),
		m_Radius(radius),
		m_TotalLife(life_time),
		m_CurrLife(life_time)
	{

	}

	int		m_NumDivs;

	float m_MasterRotateRate;
	float m_InnerRotateRate;
	float m_Radius;

	float m_TotalLife;
	float m_CurrLife;
};


//*******************************************************************************
//*******************************************************************************
class BasicApp : public AppBasic 
{
public:
	void setup();
	void update();
	void draw();
	void keyDown( KeyEvent event );

	std::list<CLayer*> m_Layers;
	CSoundEngine* mp_SoundEngine;
};

//*******************************************************************************
void BasicApp::setup()
{
	//	gl::enableDepthWrite();
	//	gl::enableDepthRead();

	glCullFace(GL_FRONT_FACE);
	glEnable(GL_CULL_FACE);
	gl::enableAlphaBlending();

	setFrameRate(60);

	for(int i=0; i<10; ++i)
	{
		m_Layers.push_back(CLayer::CreateRandom());
	}

	CSoundEngine::Create(this);
}

//*******************************************************************************
void BasicApp::keyDown(ci::app::KeyEvent event)
{
	switch (event.getChar()) 
	{
		case 'n':
			CSoundEngine::Get().NextFile();
			break;
			
		default:
			break;
	}
}

static int frame_count = 0;
//*******************************************************************************
void BasicApp::update()
{
	frame_count++;
	
	if(CSoundEngine::IsValid())
	{
		CSoundEngine::Get().Update();
	}

	int insert_count = 0;

	for(std::list<CLayer*>::iterator it = m_Layers.begin(); it != m_Layers.end();)
	{
		if((*it)->Update(frame_time))
		{
			++it;
		}
		else
		{
			it = m_Layers.erase(it);
			insert_count++;
		}
	}

	for(int i=0; i<insert_count; ++i)
	{
		m_Layers.push_back(CLayer::CreateRandom());
	}
}

//*******************************************************************************
void BasicApp::draw()
{
	glEnable(GL_CULL_FACE);

	static float time = 0;
	time += frame_time;

	float w = (float)getWindowWidth();
	float h = (float)getWindowHeight();

	float w_2 = w * 0.5f;
	float h_2 = h * 0.5f;

	gl::clear(ColorA::black(), true);

	glPushMatrix();
	gl::translate(Vec2f(w_2, h_2));

	for(std::list<CLayer*>::iterator it = m_Layers.begin(); it != m_Layers.end(); ++it)
	{
		//(*it)->draw(time, w, h);
	}

	gl::translate(Vec2f(-w_2, -h_2));

	glPopMatrix();


	glDisable(GL_CULL_FACE);
	int num_divs = 10;
	num_divs = CSoundEngine::Get().mFft.getBinSize();

	float blob_gap = w/(num_divs*2.0f);
	float blob_radius = blob_gap * 0.95f;

	if(!CSoundEngine::Get().mFftInit)
	{
		return;
	}
	float* fft = CSoundEngine::Get().mFft.getAmplitude();

	static float persistent_max = 0;
	float max = 0;
	for(int i=0; i<num_divs; i++)
	{
		float val = fft[i];
		if(val > max)
		{
			max = val;
		}
	}
	
	persistent_max = lerp(persistent_max, max, 0.1f);
	if(persistent_max > 0)
	{
		for(int i=0; i<num_divs; i++)
		{
			fft[i] /= persistent_max;
		}		
	}
	
	for(int i=0; i<num_divs; i++)
	{
		float i_f = i/(float)num_divs;

		gl::color(hsvToRGB(Vec3f(i_f, 1, 1)));
		float val = CSoundEngine::Get().GetAverage(i_f);
		gl::drawSolidCircle(Vec2f(w * i_f + blob_gap, h-blob_gap), val * blob_radius);
		val = fft[i];
		gl::drawSolidRect(Rectf(w * i_f, h, w * i_f + blob_gap, h-h*val));

		gl::color(hsvToRGB(Vec3f(1-i_f, 1, 0.5f)));
		val = CSoundEngine::Get().GetShortAverage(i_f);
		gl::drawSolidCircle(Vec2f(w * i_f + blob_gap, h-blob_gap), val * blob_radius);
	}
	
	gl::drawString(CSoundEngine::Get().m_CurrentFile, Vec2f(10,20));
	
	//CSoundEngine::Get().Draw();
}

// This line tells Flint to actually create the application
CINDER_APP_BASIC( BasicApp, RendererGl )