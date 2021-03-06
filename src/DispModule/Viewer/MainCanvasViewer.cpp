#include "MainCanvasViewer.h"
#include "MainCanvas.h"
#include "Bound.h"

#include <QMouseEvent>

MainCanvasViewer::MainCanvasViewer(QWidget *widget)
  : BasicViewer(widget)
{
  is_draw_actors = false;
  show_background = true;
  show_wireframe - false;

  interaction_mode = MainViewer::STATIC;
}

MainCanvasViewer::~MainCanvasViewer()
{

}

void MainCanvasViewer::draw()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  for (int i = 0; i < dispObjects.size(); ++i)
  {
    //startScreenCoordinatesSystem(true);
    if (show_background)
    {
      dynamic_cast<MainCanvas*>(dispObjects[i])->drawBackground();
    }
    //stopScreenCoordinatesSystem();

    // draw off screen camera
    // set the off screen glViewport and off screen camera matrix
    glViewport(0, 0, GLint(offScrCamera()->screenWidth()), GLint(offScrCamera()->screenHeight()));
    offScrCamera()->loadProjectionMatrix();
    offScrCamera()->loadModelViewMatrix();
    dynamic_cast<MainCanvas*>(dispObjects[i])->drawPrimitiveImg();
    // set back to on screen glViewport and camera matrix
    glViewport(0, 0, GLint(this->width()), GLint(this->height()));
    camera()->loadProjectionMatrix();
    camera()->loadModelViewMatrix();

    if (!dispObjects[i]->display())
    {
      std::cerr<<"Error when drawing object " << i << ".\n";
    }

  }

  if (is_draw_actors)
  {
    glClear(GL_DEPTH_BUFFER_BIT);
    drawActors();
  }
}

void MainCanvasViewer::init()
{
  std::cout << "Initialize Main Canvas Viewer.\n";
  BasicViewer::init();

  // Set shader
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  //glEnable(GL_CULL_FACE);

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glShadeModel(GL_FLAT);

  qglClearColor(QColor(Qt::white));
  setBackgroundColor(QColor(Qt::white));
  setForegroundColor(QColor(Qt::white));

  setSceneCenter(qglviewer::Vec(0, 0, 0));
  setSceneRadius(50);
  camera()->fitSphere(qglviewer::Vec(0, 0, 0), 5);
  camera()->setType(qglviewer::Camera::Type::PERSPECTIVE);
  camera()->setFlySpeed(0.5);

  // set offscr_camera
  offScrCamera()->setSceneCenter(qglviewer::Vec(0, 0, 0));
  offScrCamera()->setSceneRadius(50);
  offScrCamera()->fitSphere(qglviewer::Vec(0, 0, 0), 5);
  offScrCamera()->setType(qglviewer::Camera::Type::PERSPECTIVE);
  offScrCamera()->setFlySpeed(0.5);

  // forbid interaction in this viewer
  clearMouseBindings();
}

void MainCanvasViewer::getSnapShot()
{
  makeCurrent();

  for (size_t i = 0; i < dispObjects.size(); ++i)
  {
    MainCanvas* main_canvas = dynamic_cast<MainCanvas*>(dispObjects[i]);
    if (main_canvas)
    {
      // get camera info, matrix is column major
      GLfloat modelview[16];
      GLfloat projection[16];
      GLint viewport[4];
      offScrCamera()->getModelViewMatrix(modelview);
      offScrCamera()->getProjectionMatrix(projection);
      offScrCamera()->getViewport(viewport);
      main_canvas->passCameraInfo(modelview, projection, viewport);
      std::cout << "Field of View: " << offScrCamera()->fieldOfView()
        << "\tzClippingCoefficient: " << offScrCamera()->zClippingCoefficient()
        << "\tsceneRadius: " << offScrCamera()->sceneRadius() << std::endl;

      double clipping_range = 2 * offScrCamera()->zClippingCoefficient() * offScrCamera()->sceneRadius();
      double img_width = width();
      qglviewer::Vec point_1(img_width, 0, 0.5);
      qglviewer::Vec point_2(0, 0, 0.5);
      point_1 = offScrCamera()->unprojectedCoordinatesOf(point_1);
      point_2 = offScrCamera()->unprojectedCoordinatesOf(point_2);
      double world_width = (point_1 - point_2).norm();
      double z_scale = clipping_range * img_width / world_width;
      glViewport(0, 0, GLint(offScrCamera()->screenWidth()), GLint(offScrCamera()->screenHeight()));
      offScrCamera()->loadProjectionMatrix();
      offScrCamera()->loadModelViewMatrix();
      main_canvas->drawInfo(z_scale);
      glViewport(0, 0, GLint(this->width()), GLint(this->height()));
      std::cout << "z_scale: " << z_scale << std::endl;
    }
  }

  doneCurrent();
}

void MainCanvasViewer::setBackgroundImage(QString fname)
{
  makeCurrent();

  QImage img(fname);

  if (img.isNull())
  {
    qWarning("Unable to load file, unsupported file format");
    return;
  }

  int fixed_height = 800;
  int fixed_width = 800 * float(img.width()) / float(img.height());

  this->setMinimumHeight(fixed_height);
  this->setMinimumWidth(fixed_width);
  this->setMaximumHeight(fixed_height);
  this->setMaximumWidth(fixed_width);

  offscr_camera.setScreenWidthAndHeight(img.width(), img.height());

  for (size_t i = 0; i < dispObjects.size(); ++i)
  {
    MainCanvas* main_canvas = dynamic_cast<MainCanvas*>(dispObjects[i]);
    if (main_canvas)
    {
      main_canvas->setBackgroundImage(fname);
    }
  }

  doneCurrent();
}

void MainCanvasViewer::setReflectanceImage(QString fname)
{
  makeCurrent();

  for (size_t i = 0; i < dispObjects.size(); ++i)
  {
    MainCanvas* main_canvas = dynamic_cast<MainCanvas*>(dispObjects[i]);
    if (main_canvas)
    {
      main_canvas->setReflectanceImage(fname);
    }
  }

  doneCurrent();
}

void MainCanvasViewer::setSynthesisReflectance()
{
  makeCurrent();

  for (size_t i = 0; i < dispObjects.size(); ++i)
  {
    MainCanvas* main_canvas = dynamic_cast<MainCanvas*>(dispObjects[i]);
    if (main_canvas)
    {
      main_canvas->setSynthesisReflectance();
    }
  }

  doneCurrent();
}

void MainCanvasViewer::updateBuffer()
{
  makeCurrent();

  for (size_t i = 0; i < dispObjects.size(); ++i)
  {
    MainCanvas* main_canvas = dynamic_cast<MainCanvas*>(dispObjects[i]);
    if (main_canvas)
    {
      main_canvas->updateModelBuffer();
    }
  }

  doneCurrent();
}

void MainCanvasViewer::updateColorBuffer()
{
  makeCurrent();

  for (size_t i = 0; i < dispObjects.size(); ++i)
  {
    MainCanvas* main_canvas = dynamic_cast<MainCanvas*>(dispObjects[i]);
    if (main_canvas)
    {
      main_canvas->updateModelColorBuffer();
    }
  }

  doneCurrent();
}

void MainCanvasViewer::drawActors()
{
  for (decltype(actors.size()) i = 0; i < actors.size(); ++i)
  {
    actors[i].draw();
  }
}

void MainCanvasViewer::setGLActors(std::vector<GLActor>& actors)
{
  this->actors = actors;
}

void MainCanvasViewer::syncCameraToModel()
{
  for (size_t i = 0; i < dispObjects.size(); ++i)
  {
    MainCanvas* main_canvas = dynamic_cast<MainCanvas*>(dispObjects[i]);
    if (main_canvas)
    {
      // get camera info, matrix is column major
      GLfloat modelview[16];
      GLfloat projection[16];
      GLint viewport[4];
      offScrCamera()->getModelViewMatrix(modelview);
      offScrCamera()->getProjectionMatrix(projection);
      offScrCamera()->getViewport(viewport);
      main_canvas->passCameraInfo(modelview, projection, viewport);
    }
  }
}

void MainCanvasViewer::mouseReleaseEvent(QMouseEvent *e)
{
  if ((e->button() == Qt::LeftButton) && (e->modifiers() == Qt::NoButton))
  {
    if (interaction_mode == MainViewer::STATIC)
    {
      // do nothing
    }
    else if (interaction_mode == MainViewer::TAG_PLANE)
    {
      // get coordinates
      for (size_t i = 0; i < dispObjects.size(); ++i)
      {
        MainCanvas* canvas = dynamic_cast<MainCanvas*>(dispObjects[i]);
        if (canvas)
        {
          canvas->passTagPlanePos(e->x(), e->y());// I think it need to be flipped here?
        }
      }

      this->updateBuffer();
      this->updateGLOutside();
    }
  }
  else
  {
    QGLViewer::mouseReleaseEvent(e);
  }
}

void MainCanvasViewer::keyPressEvent(QKeyEvent *e)
{
  // Get event modifiers key
  const Qt::KeyboardModifiers modifiers = e->modifiers();

  // A simple switch on e->key() is not sufficient if we want to take state key into account.
  // With a switch, it would have been impossible to separate 'F' from 'CTRL+F'.
  // That's why we use imbricated if...else and a "handled" boolean.
  bool handled = false;
  if ((e->key()==Qt::Key_W) && (modifiers==Qt::NoButton))
  {
    show_wireframe = !show_wireframe;
    if (show_wireframe)
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    handled = true;
    updateGL();
  }
}

void MainCanvasViewer::clearPreviousInteractionInfo()
{
  for (size_t i = 0; i < dispObjects.size(); ++i)
  {
    MainCanvas* canvas = dynamic_cast<MainCanvas*>(dispObjects[i]);
    if (canvas)
    {
      canvas->clearInteractionInfo();
    }
  }
}

void MainCanvasViewer::updateCanvasRenderMode()
{
  for (size_t i = 0; i < dispObjects.size(); ++i)
  {
    MainCanvas* canvas = dynamic_cast<MainCanvas*>(dispObjects[i]);
    if (canvas)
    {
      canvas->setCanvasRenderMode();
    }
  }
}