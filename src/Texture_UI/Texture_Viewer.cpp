
#include "Texture_Viewer.h"
#include "Texture_Canvas.h"
#include "Bound.h"
#include <QKeyEvent>
#include "../Model/Model.h"
#include "../Model/Shape.h"
#include "../DispModule/Viewer/DispObject.h"
#include <QGLViewer/manipulatedFrame.h>
#include "PolygonMesh.h"
#include <QResizeEvent>
Texture_Viewer::Texture_Viewer(QWidget *widget)
  : BasicViewer(widget)
{
  wireframe_ = false;
  show_trackball = false;
  play_lightball = false;
  this->init();
  this->m_left_button_down_ = false;
  this->m_right_buttonm_down_ = false;
  m_edit_mode_ = -1;

 /* connect(this, SIGNAL(resizeEvent(QResizeEvent*)), this, SLOT(resize_happen(QResizeEvent*)));*/
}

Texture_Viewer::~Texture_Viewer()
{
	for (unsigned int i = 0; i < this->get_dispObjects().size();i++)
	{
		delete this->get_dispObjects()[i];
	}
}

void Texture_Viewer::resizeEvent(QResizeEvent* r)
{
	QGLViewer::resizeEvent(r);
	int w = r->size().width();
	int h = r->size().height();
	for (unsigned int i = 0; i < this->get_dispObjects().size(); i++)
	{
		dynamic_cast<Texture_Canvas*>(this->get_dispObjects()[i])->setsize(w,h);
	}
	
};
void Texture_Viewer::draw()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  drawCornerAxis();

  for (int i = 0; i < dispObjects.size(); ++i)
  {
    glDisable(GL_LIGHTING);
    drawTrackBall();
    if (!dispObjects[i]->display())
    {
      std::cerr<<"Error when drawing object " << i << ".\n";
    }
	else
	{
		dispObjects[i]->set_viewer(this);
	}
  }

 
  this->draw_points_under_mouse();
 // this->draw_mesh_points();
}

void Texture_Viewer::draw_points_under_mouse()
{
	glPointSize(3);
	glBegin(GL_POINTS);
	for (int i = 0; i < this->m_points_ubder_mouse_.size(); ++i) 
	{
			const qglviewer::Vec&  p = this->m_points_ubder_mouse_[i];
			glColor3f(1.0, 0, 0);
			glVertex3f(p.x, p.y, p.z);
	}
	glEnd();
};

void Texture_Viewer::init()
{

  BasicViewer::init();

  // Set shader
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  //glEnable(GL_CULL_FACE);

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glShadeModel(GL_FLAT);

  glClearColor(1,1,1,0);
  setBackgroundColor(QColor(Qt::white));
  setForegroundColor(QColor(Qt::white));

  setSceneCenter(qglviewer::Vec(0, 0, 0));
  setSceneRadius(50);
  camera()->fitSphere(qglviewer::Vec(0, 0, 0), 5);
  camera()->setType(qglviewer::Camera::Type::PERSPECTIVE);
  camera()->setFlySpeed(0.5);
  setWheelandMouse();
}

void Texture_Viewer::drawTrackBall()
{
  if (!show_trackball)
  {
    return;
  }

	double v1,v2;
	glBegin(GL_LINE_STRIP);
	for(double i = 0;i <= 2 * M_PI;i += (M_PI / 180))
	{
    v1 = ((dispObjects[0]->getBoundBox())->centroid).x + ((dispObjects[0]->getBoundBox())->getRadius()) * cos(i);
		v2 = ((dispObjects[0]->getBoundBox())->centroid).y + ((dispObjects[0]->getBoundBox())->getRadius()) * sin(i);
		glColor3f(0.8,0.6,0.6);
		glVertex3f(v1,v2,((dispObjects[0]->getBoundBox())->centroid).z);
	}
	glEnd();
	glBegin(GL_LINE_STRIP);
	for(double i = 0;i <= 2 * M_PI;i += (M_PI / 180))
	{
		v1 = ((dispObjects[0]->getBoundBox())->centroid).y + ((dispObjects[0]->getBoundBox())->getRadius()) * cos(i);
		v2 = ((dispObjects[0]->getBoundBox())->centroid).z + ((dispObjects[0]->getBoundBox())->getRadius()) * sin(i);
		glColor3f(0.6,0.8,0.6);
		glVertex3f(((dispObjects[0]->getBoundBox())->centroid).x,v1,v2);
	}
	glEnd();
	glBegin(GL_LINE_STRIP);
	for(double i = 0;i <= 2 * M_PI;i += (M_PI / 180))
	{
		v1 = ((dispObjects[0]->getBoundBox())->centroid).x + ((dispObjects[0]->getBoundBox())->getRadius()) * cos(i);
		v2 = ((dispObjects[0]->getBoundBox())->centroid).z + ((dispObjects[0]->getBoundBox())->getRadius()) * sin(i);
		glColor3f(0.6,0.6,0.8);
		glVertex3f(v1,((dispObjects[0]->getBoundBox())->centroid).y,v2);
	}
	glEnd();
}

void Texture_Viewer::setWheelandMouse()
{
  /////////////////////////////////////////////////////
  //         Mouse bindings customization            //
  //     Changes standard action mouse bindings      //
  /////////////////////////////////////////////////////

  //
  setMouseBinding(Qt::NoModifier, Qt::LeftButton, CAMERA, ROTATE);

  // Left and right buttons together make a camera zoom : emulates a mouse third button if needed.
  setMouseBinding(Qt::Key_Z, Qt::NoModifier, Qt::LeftButton, CAMERA, ZOOM);

  setMouseBinding(Qt::ControlModifier | Qt::ShiftModifier, Qt::RightButton, SELECT);
  setWheelBinding(Qt::NoModifier, CAMERA, MOVE_FORWARD);
  setMouseBinding(Qt::AltModifier, Qt::LeftButton, CAMERA, TRANSLATE);

  // Add custom mouse bindings description (see mousePressEvent())
  setMouseBindingDescription(Qt::NoModifier, Qt::RightButton, "Opens a camera path context menu");

  setManipulatedFrame(new qglviewer::ManipulatedFrame());
}

void Texture_Viewer::toggleLightball()
{
  
}

void Texture_Viewer::updateBuffer()
{
  makeCurrent();

  for (size_t i = 0; i < dispObjects.size(); ++i)
  {
	  Texture_Canvas* texture_canvas = dynamic_cast<Texture_Canvas*>(dispObjects[i]);
	  if (texture_canvas)
    {
		texture_canvas->updateModelBuffer();
    }
  }

  doneCurrent();
}

void Texture_Viewer::updateColorBuffer()
{
  makeCurrent();

  for (size_t i = 0; i < dispObjects.size(); ++i)
  {
	  Texture_Canvas* texture_canvas = dynamic_cast<Texture_Canvas*>(dispObjects[i]);
	  if (texture_canvas)
    {
		texture_canvas->updateModelColorBuffer();
    }
  }

  doneCurrent();
}

void Texture_Viewer::resetCamera()
{
  // assume only one dispObjects MainCanvas exist here
	makeCurrent(); // in case of opengl context problem in restoreStateFromFile();
  if (dispObjects.empty())
  {
    return;
  }

  Bound* scene_bounds = dynamic_cast<Texture_Canvas*>(dispObjects[0])->getBoundBox();

  qglviewer::Vec scene_center;
  scene_center = qglviewer::Vec((scene_bounds->minX + scene_bounds->maxX) / 2,
    (scene_bounds->minY + scene_bounds->maxY) / 2,
    (scene_bounds->minZ + scene_bounds->maxZ) / 2);

  setSceneCenter(scene_center);

  float x_span = (scene_bounds->maxX - scene_bounds->minX) / 2;
  float y_span = (scene_bounds->maxY - scene_bounds->minY) / 2;
  float z_span = (scene_bounds->maxZ - scene_bounds->minZ) / 2;
  float scene_radius = sqrt(x_span * x_span + y_span * y_span + z_span * z_span);
  //scene_radius *= 1.5;

  setSceneRadius(scene_radius);
  camera()->fitSphere(scene_center, scene_radius);



  setStateFileName(QString((dynamic_cast<Texture_Canvas*>(dispObjects[0])->getFilePath()+"/camera_info.xml").c_str()));
  if (restoreStateFromFile())
    std::cout << "Load camera info successes...\n";
  else
    std::cout << "Load camera info failed...\n";
  doneCurrent();
  // set the scene in MainCanvasViewer
}

void Texture_Viewer::drawCornerAxis()
{
  int viewport[4];
  int scissor[4];

  // The viewport and the scissor are changed to fit the lower left
  // corner. Original values are saved.
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetIntegerv(GL_SCISSOR_BOX, scissor);

  // Axis viewport size, in pixels
  const int size = 150;
  glViewport(0, 0, size, size);
  glScissor(0, 0, size, size);

  // The Z-buffer is cleared to make the axis appear over the
  // original image.
  glClear(GL_DEPTH_BUFFER_BIT);

  // Tune for best line rendering
  glDisable(GL_LIGHTING);
  glLineWidth(3.0);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-1, 1, -1, 1, -1, 1);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glMultMatrixd(camera()->orientation().inverse().matrix());

  glBegin(GL_LINES);
  glColor3f(1.0, 0.0, 0.0);
  glVertex3f(0.0, 0.0, 0.0);
  glVertex3f(1.0, 0.0, 0.0);

  glColor3f(0.0, 1.0, 0.0);
  glVertex3f(0.0, 0.0, 0.0);
  glVertex3f(0.0, 1.0, 0.0);

  glColor3f(0.0, 0.0, 1.0);
  glVertex3f(0.0, 0.0, 0.0);
  glVertex3f(0.0, 0.0, 1.0);
  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glEnable(GL_LIGHTING);

  // The viewport and the scissor are restored.
  glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void Texture_Viewer::mousePressEvent(QMouseEvent* e)
{	
	if (this->m_edit_mode_ < 0)
	{
		QGLViewer::mousePressEvent(e);
		return;
	}
	if (e->button()== Qt::LeftButton)
	{
		this->m_left_button_down_ = true;
	}
}
void Texture_Viewer::mouseDoubleClickEvent(QMouseEvent * event)
{
	if (this->m_edit_mode_ < 0)
	{
		QGLViewer::mouseDoubleClickEvent(event);
		return;
	}

};

void Texture_Viewer::mouseMoveEvent(QMouseEvent *e)
{
	if (this->m_edit_mode_ < 0)
	{
		QGLViewer::mouseMoveEvent(e);
		return;
	}
	if (this->m_left_button_down_ && dynamic_cast<Texture_Canvas*>(this->get_dispObjects()[0])->getModel())
	{
		bool b = false;
		QPoint p = e->pos();
// 		qglviewer::Vec v = this->camera()->pointUnderPixel(p, b);
// 		std::cout << b << " ";
// 		if (b )
// 		{
// 			this->m_points_ubder_mouse_.push_back(v);
// 			this->updateGL();
// 		}
		int x = p.x();
		int y = this->height() - p.y();
		if (x < 0 || x >= this->width() || y < 0 || y >=this->height())
		{
			return;
		}
		std::shared_ptr<Model> m = dynamic_cast<Texture_Canvas*>(this->get_dispObjects()[0])->getModel();
		int f_id = m->getPrimitiveIDImg().at<int>(y, x);
		std::cout << f_id << "\t";
		if (f_id  >= 0)
		{
			LG::PolygonMesh* poly_mesh = m->getShape()->getPolygonMesh();
			LG::Vec3 v_total(0, 0, 0);
			int num = 0;
			for (auto vfc : poly_mesh->vertices(LG::PolygonMesh::Face(f_id)))
			{
				LG::Vec3 v = poly_mesh->position(vfc);
				v_total = v_total + v;
				num++;
			}
			v_total = v_total / num;

			this->m_points_ubder_mouse_.push_back(qglviewer::Vec(v_total.x(), v_total.y(), v_total.z()));
					this->updateGL();
		}
		
	}
}
void Texture_Viewer::clear_selection()
{
	this->m_points_ubder_mouse_.clear();
};

void Texture_Viewer::mouseReleaseEvent(QMouseEvent* e)
{
	
	if (this->m_edit_mode_ < 0)
	{
		QGLViewer::mouseReleaseEvent(e); return;
	}

// 	if (e->button() == Qt::LeftButton)
// 	{
// 
// 	}
	this->m_left_button_down_ = false;
	this->m_right_buttonm_down_ = false; 
}

void Texture_Viewer::wheelEvent(QWheelEvent* e)
{
	//if (this->m_edit_mode_ < 0)
	{
		QGLViewer::wheelEvent(e); return;
	}
}

void Texture_Viewer::keyPressEvent(QKeyEvent *e)
{
	QGLViewer::keyPressEvent(e); return;
	
}

void Texture_Viewer::set_edit_mode(int b)
{
	this->m_edit_mode_ = b;
};
bool Texture_Viewer::get_edit_mode()
{
	return this->m_edit_mode_;
};

bool Texture_Viewer::draw_mesh_points()
{
	glBegin(GL_POINTS);
	//glColor4f(1, 1, 1, 1);
	glColor4f(1, 0, 0, 0);

	for (int i = 0; i < dispObjects.size(); ++i)
	{
		if (dispObjects[i]->getModel() == NULL)
		{
			continue;
		}
		const VertexList& vl = dispObjects[i]->getModel()->getShapeVertexList();
		for (unsigned int j = 0; j < vl.size(); j+=3)
		{
			glVertex3f(vl[j], vl[j+1], vl[j+2]);
		}
	}
	glEnd();
	return true;
};