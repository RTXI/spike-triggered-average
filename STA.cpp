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

#include <STA.h>
#include <math.h>
#include <algorithm>
#include <numeric>
#include <time.h>

#include <qdatetime.h>
#include <qfile.h>
#include <qgridview.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qcombobox.h>
#include <qhbuttongroup.h>
#include <qprinter.h>
#include <qpicture.h>
#include <qpainter.h>

#include <qpushbutton.h>
#include <qtimer.h>
#include <qtooltip.h>
#include <qvalidator.h>
#include <qvbox.h>
#include <qwhatsthis.h>
#include <qtextstream.h>
#include <sys/stat.h>
#include <qwt-qt3/qwt_plot.h>
#include <qwt-qt3/qwt_plot_curve.h>
#if QT_VERSION >= 0x040300
#ifdef QT_SVG_LIB
#include <qsvggenerator.h>
#endif
#endif
#if QT_VERSION >= 0x040000
#include <qprintdialog.h>
#include <qfileinfo.h>
#else
#include <qwt-qt3/qwt_painter.h>
#endif

extern "C" Plugin::Object *
createRTXIPlugin(void)
{
  return new STA();
}

static DefaultGUIModel::variable_t vars[] =
  {
    { "Input", "Quantity to compute the spike-triggered average for", DefaultGUIModel::INPUT, },
    { "Event Trigger", "trigger that indicates the spike time/event (=1)", DefaultGUIModel::INPUT, },
    { "Left Window (s)", "Amount of time before the spike to include in average",
        DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "Right Window (s)", "Amount of time after the spike to include in average",
        DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "Plot y-min", "Minimum for y-axis on the plot", DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "Plot y-max", "Minimum for y-axis on the plot", DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
    { "Event Count", "Number of spikes/events included in the current average", DefaultGUIModel::STATE, },
    { "Time (s)", "Time (s)", DefaultGUIModel::STATE, }, };

static size_t num_vars = sizeof(vars) / sizeof(DefaultGUIModel::variable_t);

// Default constructor
STA::STA(void) :
  DefaultGUIModel("STA", ::vars, ::num_vars)
{

  QWhatsThis::add(
      this,
      "<p><b>STA:</b></p><p> This plug-in computes an event-triggered average of the input signal. The event trigger should provide a value of 1. The averaged signal will update periodically. Click and drag on the plot to resize the axes.</p>");
  initParameters();
  createGUI(vars, num_vars); // this is required to create the GUI
  update( INIT);
  refresh(); // this is required to update the GUI with parameter and state values
  //emit setPlotRange(-leftwintime, rightwintime, plotymin, plotymax);
  //refreshSTA();
  printf("\nStarting Event-Triggered Average:\n"); // prints to terminal

}

void
STA::createGUI(DefaultGUIModel::variable_t *var, int size)
{
  setMinimumSize(200, 300); // Qt API for setting window size

  QBoxLayout *layout = new QHBoxLayout(this); // overall GUI layout

  // Left side GUI
  QBoxLayout *leftlayout = new QVBoxLayout();

  // create custom GUI components
  rplot = new BasicPlot(this);
  rCurve = new QwtPlotCurve("Curve 1");
  rCurve->attach(rplot);
  rCurve->setPen(QColor(Qt::white));

  QBoxLayout *rightlayout = new QVBoxLayout();
  QHButtonGroup *plotBox = new QHButtonGroup("Event-triggered Average Plot:",
      this);
  QPushButton *clearButton = new QPushButton("&Clear", plotBox);
  QPushButton *savePlotButton = new QPushButton("Save Screenshot", plotBox);
  QPushButton *printButton = new QPushButton("Print", plotBox);
  QPushButton *saveDataButton = new QPushButton("Save Data", plotBox);

  QObject::connect(clearButton, SIGNAL(clicked()), this, SLOT(clearData()));
  QObject::connect(savePlotButton, SIGNAL(clicked()), this, SLOT(exportSVG()));
  QObject::connect(printButton, SIGNAL(clicked()), this, SLOT(print()));
  QObject::connect(saveDataButton, SIGNAL(clicked()), this, SLOT(saveData()));

  rightlayout->addWidget(plotBox);
  rightlayout->addWidget(rplot);
  QObject::connect(this, SIGNAL(setPlotRange(double, double, double, double)), rplot,
      SLOT(setAxes(double, double, double, double)));

  // Add custom left side GUI components to layout above default_gui_model components

  // Create default_gui_model GUI DO NOT EDIT
  QScrollView *sv = new QScrollView(this);
  sv->setResizePolicy(QScrollView::AutoOneFit);
  leftlayout->addWidget(sv);

  QWidget *viewport = new QWidget(sv->viewport());
  sv->addChild(viewport);
  QGridLayout *scrollLayout = new QGridLayout(viewport, 1, 2);

  size_t nstate = 0, nparam = 0, nevent = 0, ncomment = 0;
  for (size_t i = 0; i < num_vars; i++)
    {
      if (vars[i].flags & (PARAMETER | STATE | EVENT | COMMENT))
        {
          param_t param;

          param.label = new QLabel(vars[i].name, viewport);
          scrollLayout->addWidget(param.label, parameter.size(), 0);
          param.edit = new DefaultGUILineEdit(viewport);
          scrollLayout->addWidget(param.edit, parameter.size(), 1);

          QToolTip::add(param.label, vars[i].description);
          QToolTip::add(param.edit, vars[i].description);

          if (vars[i].flags & PARAMETER)
            {
              if (vars[i].flags & DOUBLE)
                {
                  param.edit->setValidator(new QDoubleValidator(param.edit));
                  param.type = PARAMETER | DOUBLE;
                }
              else if (vars[i].flags & UINTEGER)
                {
                  QIntValidator *validator = new QIntValidator(param.edit);
                  param.edit->setValidator(validator);
                  validator->setBottom(0);
                  param.type = PARAMETER | UINTEGER;
                }
              else if (vars[i].flags & INTEGER)
                {
                  param.edit->setValidator(new QIntValidator(param.edit));
                  param.type = PARAMETER | INTEGER;
                }
              else
                param.type = PARAMETER;
              param.index = nparam++;
              param.str_value = new QString;
            }
          else if (vars[i].flags & STATE)
            {
              param.edit->setReadOnly(true);
              param.edit->setPaletteForegroundColor(Qt::darkGray);
              param.type = STATE;
              param.index = nstate++;
            }
          else if (vars[i].flags & EVENT)
            {
              param.edit->setReadOnly(true);
              param.type = EVENT;
              param.index = nevent++;
            }
          else if (vars[i].flags & COMMENT)
            {
              param.type = COMMENT;
              param.index = ncomment++;
            }

          parameter[vars[i].name] = param;
        }
    }

  QHBox *utilityBox = new QHBox(this);
  pauseButton = new QPushButton("Pause", utilityBox);
  pauseButton->setToggleButton(true);
  QObject::connect(pauseButton,SIGNAL(toggled(bool)),this,SLOT(pause(bool)));
  QObject::connect(pauseButton,SIGNAL(toggled(bool)),savePlotButton,SLOT(setEnabled(bool)));
  QObject::connect(pauseButton,SIGNAL(toggled(bool)),printButton,SLOT(setEnabled(bool)));
  QObject::connect(pauseButton,SIGNAL(toggled(bool)),saveDataButton,SLOT(setEnabled(bool)));
  QPushButton *modifyButton = new QPushButton("Modify", utilityBox);
  QObject::connect(modifyButton,SIGNAL(clicked(void)),this,SLOT(modify(void)));
  QPushButton *unloadButton = new QPushButton("Unload", utilityBox);
  QObject::connect(unloadButton,SIGNAL(clicked(void)),this,SLOT(exit(void)));
  QObject::connect(pauseButton,SIGNAL(toggled(bool)),modifyButton,SLOT(setEnabled(bool)));
  QToolTip::add(pauseButton, "Start/Stop current clamp protocol");
  QToolTip::add(modifyButton, "Commit changes to parameter values");
  QToolTip::add(unloadButton, "Close module");

  // add custom components to layout below default_gui_model components
  leftlayout->addWidget(utilityBox);
  // Add left and right side layouts to the overall layout
  layout->addLayout(leftlayout);
  //	layout->setResizeMode(QLayout::Fixed);
  layout->addLayout(rightlayout);
  leftlayout->setResizeMode(QLayout::Fixed);
  layout->setStretchFactor(rightlayout, 4);
  layout->setResizeMode(QLayout::FreeResize);

  // set GUI refresh rate
  QTimer *timer2 = new QTimer(this);
  timer2->start(2000);
  // set STA plot refresh rate
  QObject::connect(timer2, SIGNAL(timeout(void)), this, SLOT(refreshSTA(void)));

  show();

}

STA::~STA(void)
{
}

void
STA::execute(void)
{
  systime = count * dt; // current time,
  signalin.push_back(input(0)); // always buffer, we don't know when the event occurs

  if (triggered == 1)
    {
      wincount++;
      if (wincount == rightwin)
        { // compute average, do this way to keep numerical accuracy
          for (int i = 0; i < leftwin + rightwin + 1; i++)
            {
              stasum[i] = stasum[i] + signalin[i];
              staavg[i] = stasum[i] / eventcount;
            }
        }
      else if (wincount > rightwin)
        {
          wincount = 0;
          triggered = 0;
        }
    }
  else if (triggered == 0 && input(1) == 1)
    {
      triggered = 1;
      eventcount++;
    }

  count++; // increment count to measure time
  return;
}

void
STA::update(DefaultGUIModel::update_flags_t flag)
{
  switch (flag)
    {
  case INIT:
    setParameter("Left Window (s)", QString::number(leftwintime));
    setParameter("Right Window (s)", QString::number(rightwintime));
    setParameter("Plot y-min", QString::number(plotymin));
    setParameter("Plot y-max", QString::number(plotymax));
    setState("Time (s)", systime);
    setState("Event Count", eventcount);
    break;
  case MODIFY:
    leftwintime = getParameter("Left Window (s)").toDouble();
    rightwintime = getParameter("Right Window (s)").toDouble();
    plotymin = getParameter("Plot y-min").toDouble();
    plotymax = getParameter("Plot y-max").toDouble();
    bookkeep();
    break;
  case PAUSE:
    output(0) = 0; // stop command in case pause occurs in the middle of command
    printf("Protocol paused.\n");
    break;
  case UNPAUSE:
    bookkeep();
    printf("Protocol started.\n");
    break;
  case PERIOD:
    dt = RT::System::getInstance()->getPeriod() * 1e-9;
    bookkeep();
  default:
    break;
    }
}

// custom functions

void
STA::initParameters()
{
  dt = RT::System::getInstance()->getPeriod() * 1e-9; // s
  signalin.clear();
  assert(signalin.size() == 0);
  leftwintime = 0.050; // s
  rightwintime = 0.050;
  plotymax = 0.050;
  plotymin = -0.100;
  bookkeep();

}

void
STA::bookkeep()
{
  triggered = 0;
  count = 0;
  eventcount = 0;
  wincount = 0;
  systime = 0;
  n = leftwin + rightwin + 1;
  leftwin = int(leftwintime / dt);
  rightwin = int(rightwintime / dt);
  signalin.rset_capacity(n);
  staavg.resize(n);
  stasum.resize(n);
  time.resize(n);
  for (int i = 0; i < n; i++)
    {
      signalin.push_back(0);
      stasum[i] = 0;
      staavg[i] = 0;
      time[i] = dt * i - leftwintime;
    }
  //emit setPlotRange(-leftwintime, rightwintime, plotymin, plotymax);
  //refreshSTA();

}

void
STA::refreshSTA()
{

  rCurve->setData(time, staavg, n);
  rplot->replot();
  emit setPlotRange(-leftwintime, rightwintime, plotymin, plotymax);
  /*
   emit setPlotRange(rplot->axisScaleDiv(QwtPlot::xBottom)->lowerBound(),
   rplot->axisScaleDiv(QwtPlot::xBottom)->upperBound(), rplot->axisScaleDiv(
   QwtPlot::yLeft)->lowerBound(),
   rplot->axisScaleDiv(QwtPlot::yLeft)->upperBound());
   */
}

void
STA::clearData()
{
  eventcount = 0;
  for (int i = 0; i < n; i++)
    {
      signalin.push_back(0);
    }
  triggered = 0;
  wincount = 0;
  for (int i = 0; i < n; i++)
    {
      stasum[i] = 0;
      staavg[i] = 0;
    }

  rCurve->setData(time, staavg, n);
  rplot->replot();
}

void
STA::saveData()
{
  QFileDialog* fd = new QFileDialog(this, "Save File As", TRUE);
  fd->setMode(QFileDialog::AnyFile);
  fd->setViewMode(QFileDialog::Detail);
  QString fileName;
  if (fd->exec() == QDialog::Accepted)
    {
      fileName = fd->selectedFile();

      if (OpenFile(fileName))
        {
          for (int i = 0; i < n; i++)
            {
              stream << (double) time[i] << " " << (double) staavg[i] << "\n";
            }
          dataFile.close();
          printf("File closed.\n");
        }
      else
        {
          QMessageBox::information(this,
              "Event-triggered Average: Save Average",
              "There was an error writing to this file. You can view\n"
                "the values that should be plotted in the terminal.\n");
        }
    }
}

bool
STA::OpenFile(QString FName)
{
  dataFile.setName(FName);
  if (dataFile.exists())
    {
      switch (QMessageBox::warning(this, "Event-triggered Average", tr(
          "This file already exists: %1.\n").arg(FName), "Overwrite", "Append",
          "Cancel", 0, 2))
        {
      case 0: // overwrite
        dataFile.remove();
        if (!dataFile.open(IO_Raw | IO_WriteOnly))
          {
            return false;
          }
        break;
      case 1: // append
        if (!dataFile.open(IO_Raw | IO_WriteOnly | IO_Append))
          {
            return false;
          }
        break;
      case 2: // cancel
        return false;
        break;
        }
    }
  else
    {
      if (!dataFile.open(IO_Raw | IO_WriteOnly))
        return false;
    }
  stream.setDevice(&dataFile);
  printf("File opened: %s\n", FName.latin1());
  return true;
}

void
STA::print()
{
#if 1
  QPrinter printer;
#else
  QPrinter printer(QPrinter::HighResolution);
#if QT_VERSION < 0x040000
  printer.setOutputToFile(true);
  printer.setOutputFileName("/tmp/STA.ps");
  printer.setColorMode(QPrinter::Color);
#else
  printer.setOutputFileName("/tmp/STA.pdf");
#endif
#endif

  QString docName = rplot->title().text();
  if (!docName.isEmpty())
    {
      docName.replace(QRegExp(QString::fromLatin1("\n")), tr(" -- "));
      printer.setDocName(docName);
    }

  printer.setCreator("RTXI");
  printer.setOrientation(QPrinter::Landscape);

#if QT_VERSION >= 0x040000
  QPrintDialog dialog(&printer);
  if ( dialog.exec() )
    {
#else
  if (printer.setup())
    {
#endif
      RTXIPrintFilter filter;
      if (printer.colorMode() == QPrinter::GrayScale)
        {
          int options = QwtPlotPrintFilter::PrintAll;
          filter.setOptions(options);
          filter.color(QColor(29, 100, 141),
              QwtPlotPrintFilter::CanvasBackground);
          filter.color(Qt::white, QwtPlotPrintFilter::CurveSymbol);
        }
      rplot->print(printer, filter);
    }
}

void
STA::exportSVG()
{
  QString fileName = "STA.svg";

#if QT_VERSION < 0x040000

#ifndef QT_NO_FILEDIALOG
  fileName = QFileDialog::getSaveFileName("STA.svg", "SVG Documents (*.svg)",
      this);
#endif
  if (!fileName.isEmpty())
    {
      // enable workaround for Qt3 misalignments
      QwtPainter::setSVGMode(true);
      QPicture picture;
      QPainter p(&picture);
      rplot->print(&p, QRect(0, 0, 800, 600));
      p.end();
      picture.save(fileName, "svg");
    }

#elif QT_VERSION >= 0x040300

#ifdef QT_SVG_LIB
#ifndef QT_NO_FILEDIALOG
  fileName = QFileDialog::getSaveFileName(
      this, "Export File Name", QString(),
      "SVG Documents (*.svg)");
#endif
  if ( !fileName.isEmpty() )
    {
      QSvgGenerator generator;
      generator.setFileName(fileName);
      generator.setSize(QSize(800, 600));
      rplot->print(generator);
    }
#endif
#endif
}
