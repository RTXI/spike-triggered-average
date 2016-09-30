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

#include "spike-triggered-average.h"
#include <math.h>
#include <algorithm>
#include <numeric>
#include <time.h>

#include <sys/stat.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>

#include <QSvgGenerator>

#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>
#include <QFileInfo>

extern "C" Plugin::Object *createRTXIPlugin(void) {
	return new STA();
}

static DefaultGUIModel::variable_t vars[] = {
	{ "Input", "Quantity to compute the spike-triggered average for", DefaultGUIModel::INPUT, },
	{ "Event Trigger", "trigger that indicates the spike time/event (=1)", DefaultGUIModel::INPUT, },
	{ "Trigger Threshold (V)", "Threshold for detecting a trigger in volts", DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
	{ "Interval (s)", "Minimum time between events", DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
	{ "Plot X-min (s)", "Amount of time before the spike to include in average",
		DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
	{ "Plot X-max (s)", "Amount of time after the spike to include in average",
		DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
	{ "Plot Y-min (V)", "Minimum for y-axis on the plot", DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
	{ "Plot Y-max (V)", "Minimum for y-axis on the plot", DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
	{ "Plot Refresh (ms)", "Interval for updating plot", DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE, },
	{ "Event Count", "Number of spikes/events included in the current average", DefaultGUIModel::STATE, },
	{ "Time (s)", "Time (s)", DefaultGUIModel::STATE, },
};

static size_t num_vars = sizeof(vars) / sizeof(DefaultGUIModel::variable_t);

// Default constructor
STA::STA(void) : DefaultGUIModel("Spike-Triggered Average", ::vars, ::num_vars) {
	setWhatsThis(
			"<p><b>STA:</b></p><p> This plug-in computes an event-triggered average of the input signal. The event trigger should provide a value of 1. The averaged signal will update periodically. Click and drag on the plot to resize the axes.</p>");
	initParameters();
	DefaultGUIModel::createGUI(vars, num_vars); // this is required to create the GUI
	customizeGUI();
	update( INIT );
	refresh();
	emit setPlotRange(-leftwintime, rightwintime, plotymin, plotymax);
	refreshSTA();
	QTimer::singleShot(0, this, SLOT(resizeMe()));
}

void STA::customizeGUI(void) {
	QGridLayout *customLayout = DefaultGUIModel::getLayout();
	customLayout->setColumnStretch(1, 1);

	rplot = new BasicPlot(this);
	rCurve = new QwtPlotCurve("Average");
	rCurve->attach(rplot);
	rCurve->setPen(QColor(Qt::red));

	QVBoxLayout *rightLayout = new QVBoxLayout;
	QGroupBox *plotBox = new QGroupBox("Event-triggered Average Plot");
	QHBoxLayout *plotBoxLayout = new QHBoxLayout;
	QVBoxLayout *plotBoxOneLayout = new QVBoxLayout;
	QVBoxLayout *plotBoxTwoLayout = new QVBoxLayout;
	plotBox->setLayout(plotBoxLayout);
	plotBoxLayout->addLayout(plotBoxOneLayout);
	plotBoxLayout->addLayout(plotBoxTwoLayout);
	QPushButton *clearButton = new QPushButton("&Clear");
	QPushButton *savePlotButton = new QPushButton("Screenshot");
	QPushButton *printButton = new QPushButton("Print");
	QPushButton *saveDataButton = new QPushButton("Save Data");
	plotBoxOneLayout->addWidget(clearButton);
	plotBoxOneLayout->addWidget(savePlotButton);
	plotBoxTwoLayout->addWidget(printButton);
	plotBoxTwoLayout->addWidget(saveDataButton);

	rightLayout->addWidget(rplot);

	QObject::connect(clearButton, SIGNAL(clicked()), this, SLOT(clearData()));
	QObject::connect(savePlotButton, SIGNAL(clicked()), this, SLOT(exportSVG()));
	QObject::connect(printButton, SIGNAL(clicked()), this, SLOT(print()));
	QObject::connect(saveDataButton, SIGNAL(clicked()), this, SLOT(saveData()));
	QObject::connect(DefaultGUIModel::pauseButton, SIGNAL(toggled(bool)),this,SLOT(pause(bool)));
	QObject::connect(DefaultGUIModel::pauseButton, SIGNAL(toggled(bool)),savePlotButton,SLOT(setEnabled(bool)));
	QObject::connect(DefaultGUIModel::pauseButton, SIGNAL(toggled(bool)),printButton,SLOT(setEnabled(bool)));
	QObject::connect(DefaultGUIModel::pauseButton, SIGNAL(toggled(bool)),saveDataButton,SLOT(setEnabled(bool)));
	//QObject::connect(DefaultGUIModel::pauseButton, SIGNAL(toggled(bool)),DefaultGUIModel::modifyButton,SLOT(setEnabled(bool)));
	QObject::connect(this, SIGNAL(setPlotRange(double, double, double, double)), rplot, SLOT(setAxes(double, double, double, double)));

	DefaultGUIModel::pauseButton->setToolTip("Start/Step current clamp protocol");
	DefaultGUIModel::modifyButton->setToolTip("Commit changes to parameter values");
	DefaultGUIModel::unloadButton->setToolTip("Close module");

	timer2 = new QTimer(this);
	timer2->start(plotRefresh);
	QObject::connect(timer2, SIGNAL(timeout(void)), this, SLOT(refreshSTA(void)));

	customLayout->addWidget(plotBox, 0, 0, 1, 1);
	customLayout->addLayout(rightLayout, 0, 1, 11, 1);
	setLayout(customLayout);
}

STA::~STA(void) {}

void STA::execute(void) {
	systime = count * dt; // current time,
	signalin.push_back(input(0)); // always buffer, we don't know when the event occurs

	if (triggered == 1) {
		wincount++;
		if (wincount == rightwin) { // compute average, do this way to keep numerical accuracy
			for (int i = 0; i < leftwin + rightwin + 1; i++) {
				stasum[i] = stasum[i] + signalin[i];
				staavg[i] = stasum[i] / eventcount;
			}
		} else if (wincount > rightwin) {
			wincount = 0;
			triggered = 0;
		}
	} else if (triggered == 0 && input(1) >= threshold &&
			(RT::OS::getTime() - last_event_time > interval)) {
		last_event_time = RT::OS::getTime();
		triggered = 1;
		eventcount++;
	}

	count++; // increment count to measure time
	return;
}

void STA::update(DefaultGUIModel::update_flags_t flag) {
	switch (flag) {
		case INIT:
			setParameter("Interval (s)", interval*1e-9); // display s
			setParameter("Trigger Threshold (V)", threshold);
			setParameter("Plot Refresh (ms)", plotRefresh);
			setParameter("Plot X-min (s)", QString::number(leftwintime));
			setParameter("Plot X-max (s)", QString::number(rightwintime));
			setParameter("Plot Y-min (V)", QString::number(plotymin));
			setParameter("Plot Y-max (V)", QString::number(plotymax));
			setState("Time (s)", systime);
			setState("Event Count", eventcount);
			break;

		case MODIFY:
			interval = getParameter("Interval (s)").toDouble()*1e9; // use ns
			threshold = getParameter("Trigger Threshold (V)").toDouble();
			plotRefresh = getParameter("Plot Refresh (ms)").toDouble();
			leftwintime = getParameter("Plot X-min (s)").toDouble();
			rightwintime = getParameter("Plot X-max (s)").toDouble();
			plotymin = getParameter("Plot Y-min (V)").toDouble();
			plotymax = getParameter("Plot Y-max (V)").toDouble();
			timer2->start(plotRefresh);
			bookkeep();
			break;

		case PAUSE:
			break;

		case UNPAUSE:
			bookkeep();
			break;

		case PERIOD:
			dt = RT::System::getInstance()->getPeriod() * 1e-9;
			bookkeep();

		default:
			break;
	}
}

// custom functions
void STA::initParameters() {
	dt = RT::System::getInstance()->getPeriod() * 1e-9; // s
	signalin.clear();
	assert(signalin.size() == 0);
	threshold = 1; // V
	interval = 0.5*1e9; // ns
	last_event_time = 0;
	plotRefresh = 1000; // ms
	leftwintime = 0.050; // s
	rightwintime = 0.050;
	plotymax = 0.050;
	plotymin = -0.100;
	bookkeep();
}

void STA::bookkeep() {
	last_event_time = 0;
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
	for (int i = 0; i < n; i++) {
		signalin.push_back(0);
		stasum[i] = 0;
		staavg[i] = 0;
		time[i] = dt * i - leftwintime;
	}
	emit setPlotRange(-leftwintime, rightwintime, plotymin, plotymax);
}

void STA::refreshSTA() {
	rCurve->setSamples(time, staavg);
	rplot->replot();
}

void STA::clearData() {
	eventcount = 0;
	for (int i = 0; i < n; i++) {
		signalin.push_back(0);
	}
	triggered = 0;
	wincount = 0;
	for (int i = 0; i < n; i++) {
		stasum[i] = 0;
		staavg[i] = 0;
	}

	rCurve->setSamples(time, staavg);
	rplot->replot();
}

void STA::saveData() {
	QFileDialog* fd = new QFileDialog(this);
	fd->setFileMode(QFileDialog::AnyFile);
	fd->setViewMode(QFileDialog::Detail);
	QStringList fileList;
	QString fileName;
	if (fd->exec() == QDialog::Accepted) {
		fileList = fd->selectedFiles();
		if (!fileList.isEmpty()) fileName = fileList[0];

		if (OpenFile(fileName)) {
			for (int i = 0; i < n; i++) {
				stream << (double) time[i] << " " << (double) staavg[i] << "\n";
			}
			dataFile.close();
		} else {
			QMessageBox::information(this,
					"Event-triggered Average: Save Average",
					"There was an error writing to this file. You can view\n"
					"the values that should be plotted in the terminal.\n");
		}
	}
}

bool STA::OpenFile(QString FName) {
	dataFile.setFileName(FName);
	if (dataFile.exists()) {
		switch (QMessageBox::warning(this, "Event-triggered Average", tr(
						"This file already exists: %1.\n").arg(FName), "Overwrite", "Append",
					"Cancel", 0, 2)) {
			case 0: // overwrite
				dataFile.remove();
				if (!dataFile.open(QIODevice::Unbuffered | QIODevice::WriteOnly)) return false;
				break;

			case 1: // append
				if (!dataFile.open(QIODevice::Unbuffered | QIODevice::Append )) return false;
				break;

			case 2: // cancel
				return false;
				break;
		}
	} else {
		if (!dataFile.open(QIODevice::Unbuffered | QIODevice::WriteOnly))	return false;
	}
	stream.setDevice(&dataFile);
	return true;
}

void STA::print() {
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
	if (!docName.isEmpty()) {
		docName.replace(QRegExp(QString::fromLatin1("\n")), tr(" -- "));
		printer.setDocName(docName);
	}

	printer.setCreator("RTXI");
	printer.setOrientation(QPrinter::Landscape);

#if QT_VERSION >= 0x040000
	QPrintDialog dialog(&printer);
	if ( dialog.exec() ) {
#else
		if (printer.setup()) {
#endif
			QwtPlotRenderer *renderer = new QwtPlotRenderer;
			renderer->renderTo(rplot, printer);
		}
	}

void STA::exportSVG() {
	QString fileName = "STA.svg";

#if QT_VERSION < 0x040000

#ifndef QT_NO_FILEDIALOG
	fileName = QFileDialog::getSaveFileName("STA.svg", "SVG Documents (*.svg)", this);
#endif
	if (!fileName.isEmpty()) {
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
	fileName = QFileDialog::getSaveFileName(this, "Export File Name", QString(),
			"SVG Documents (*.svg)");
#endif
	if ( !fileName.isEmpty() ) {
		QSvgGenerator generator;
		generator.setFileName(fileName);
		generator.setSize(QSize(800, 600));
		rplot->print(generator);
	}
#endif
#endif
}
