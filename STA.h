/*
 Copyright (C) 2011 Georgia Institute of Technology

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

/*
 * STA
 * Computes an event-triggered average of an input signal eg. a spike-triggered average using SpikeDetect.
 * Requires boost library for circular buffer.
 */

#include <default_gui_model.h>
#include <boost/circular_buffer.hpp>
#include "include/basicplot.h"
#include "include/RTXIprintfilter.h"
#include <qstring.h>
#include <cstdlib>
#include <qmessagebox.h>
#include <qcombobox.h>
#include <qwt-qt3/qwt_plot_curve.h>
#include <qwt-qt3/qwt_array.h>
#include <qfiledialog.h>

class STA : public DefaultGUIModel
{

Q_OBJECT

public:

  STA(void);
  virtual
  ~STA(void);
  void
  execute(void);
  void
  createGUI(DefaultGUIModel::variable_t *, int);

public slots:

signals: // all custom signals
void setPlotRange(double newxmin, double newxmax, double newymin, double newymax);

protected:

  virtual void
  update(DefaultGUIModel::update_flags_t);

private:
  // inputs, states
  boost::circular_buffer<double> signalin; // buffer for data before event
  double dt;
  long long count; // keep track of plug-in time
  double systime;
  int triggered;
  QwtArray<double> staavg;
  QwtArray<double> stasum;
  QwtArray<double> time;
  double eventcount; // number of events
  int wincount; // keep track of time since event

  double leftwintime; // units of time
  double rightwintime;
  int leftwin; // number of timesteps
  int rightwin;
  int n;
  double plotymin;
  double plotymax;

  // QT components
  BasicPlot *rplot;
  QwtPlotCurve *rCurve;

  // STA functions
  void
  initParameters();
  void
  bookkeep();
  bool
  OpenFile(QString);
  QFile dataFile;
  QTextStream stream;

private slots:
  // all custom slots
  void refreshSTA(void);
  void
  clearData();
  void
  saveData();
  void
  print();
  void
  exportSVG();

};
