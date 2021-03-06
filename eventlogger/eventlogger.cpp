#include "../msg/msg.h"
#include "../utils/utils.h"
#include <dashel/dashel.h>
#include <cmath>
#include <QtGui>
#include <cassert>
#include <fstream>
#include <iostream>

#ifdef _MSC_VER
#define QWT_DLL
#endif // _MSC_VER

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_legend.h>

using namespace Dashel;
using namespace Aseba;
using namespace std;

class EventDataWrapper : public QwtData
{
private:
	vector<double>& _x;
	vector<sint16>& _y;
	
public:
	EventDataWrapper(vector<double>& _x, vector<sint16>& _y) :
		_x(_x),
		_y(_y)
	{ }
	virtual QwtData *   copy () const { return new EventDataWrapper(*this); }
	virtual size_t   size () const { return _x.size(); }
	virtual double x (size_t i) const { return _x[i]; }
	virtual double y (size_t i) const { return (double)_y[i]; }
};

class EventLogger : public Hub, public QwtPlot
{
protected:
	Stream* stream;
	int eventId;
	vector<vector<sint16> > values;
	vector<double> timeStamps;
	QTime startingTime;
	ofstream outputFile;
	
public:
	EventLogger(const char* target, int eventId, int eventVariablesCount, const char* filename) :
		QwtPlot(QwtText(QString(tr("Plot for event %0")).arg(eventId))),
		eventId(eventId),
		values(eventVariablesCount)
	{
		stream = Hub::connect(target);
		cout << "Connected to " << stream->getTargetName() << endl;
		
		startingTime = QTime::currentTime();
		
		setCanvasBackground(Qt::white);
		setAxisTitle(xBottom, tr("Time (seconds)"));
		setAxisTitle(yLeft, tr("Values"));
		
		QwtLegend *legend = new QwtLegend;
		//legend->setItemMode(QwtLegend::CheckableItem);
		insertLegend(legend, QwtPlot::BottomLegend);
		
		for (size_t i = 0; i < values.size(); i++)
		{
			QwtPlotCurve *curve = new QwtPlotCurve(QString("%0").arg(i));
			curve->setData(EventDataWrapper(timeStamps, values[i]));
			curve->attach(this);
			curve->setPen(QColor::fromHsv((i * 360) / values.size(), 255, 100));
		}
		
		resize(1000, 600);
		
		if (filename)
			outputFile.open(filename);
		
		startTimer(10);
	}

protected:
	virtual void timerEvent ( QTimerEvent * event )
	{
		if (!step(0))
			close();
	}
	
	void incomingData(Stream *stream)
	{
		Message *message = Message::receive(stream);
		UserMessage *userMessage = dynamic_cast<UserMessage *>(message);
		if (userMessage)
		{
			if (userMessage->type == eventId)
			{
				double elapsedTime = (double)startingTime.msecsTo(QTime::currentTime()) / 1000.;
				if (outputFile.is_open())
					outputFile << elapsedTime;
				timeStamps.push_back(elapsedTime);
				for (size_t i = 0; i < values.size(); i++)
				{
					if (i < userMessage->data.size())
					{
						if (outputFile.is_open())
							outputFile << " " << userMessage->data[i];
						values[i].push_back(userMessage->data[i]);
					}
					else
					{
						if (outputFile.is_open())
							outputFile << " " << 0;
						values[i].push_back(0);
					}
				}
				if (outputFile.is_open())
					outputFile << endl;
				replot();
			}
		}
		delete message;
	}
	
	void connectionClosed(Stream *stream, bool abnormal)
	{
		dumpTime(cerr);
		cout << "Connection closed to " << stream->getTargetName();
		if (abnormal)
			cout << " : " << stream->getFailReason();
		cout << endl;
		stop();
	}
};

int main(int argc, char *argv[])
{
	if (argc < 4)
	{
		cerr << "Usage " << argv[0] << " target event_id event_variables_count [output file]" << endl;
		return 1;
	}
	
	QApplication app(argc, argv);
	
	int res;
	try
	{
		EventLogger logger(argv[1], atoi(argv[2]), atoi(argv[3]), (argc > 4 ? argv[4] : 0));
		logger.show();
		res = app.exec();
	}
	catch(Dashel::DashelException e)
	{
		std::cerr << e.what() << std::endl;
		return 2;
	}
	
	return res;
}
