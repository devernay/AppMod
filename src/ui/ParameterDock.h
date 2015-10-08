#ifndef ParameterDock_H
#define ParameterDock_H

#include <QDockWidget>
#include "ui_ParameterDock.h"

#include <memory>

class DispModuleHandler;

class ParameterDock : public QDockWidget, public Ui::ParameterDock
{
  Q_OBJECT;

public:
  ParameterDock();
  ~ParameterDock();

  void setDispModules(std::shared_ptr<DispModuleHandler> modules);

private slots:
  void setFeatureRender(int state);
  void setEdgeThreshold(int val);
  void setUseFlat(int state);
  void showAllLines(int state);
  void showProjCrsp(int state);

private:
  std::shared_ptr<DispModuleHandler> disp_modules;
};

#endif // !ParameterDock_H
