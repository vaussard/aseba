
/*
	Aseba - an event-based middleware for distributed robot control
	Copyright (C) 2006 - 2007:
		Stephane Magnenat <stephane at magnenat dot net>
		(http://stephane.magnenat.net)
		Valentin Longchamp <valentin dot longchamp at epfl dot ch>
		Laboratory of Robotics Systems, EPFL, Lausanne
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	any other version as decided by the two original authors
	Stephane Magnenat and Valentin Longchamp.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TCPTARGET_H
#define TCPTARGET_H

#include "Target.h"
#include "../common/consts.h"
#include <QString>
#include <QDialog>
#include <map>
#include <dashel/dashel.h>

class QPushButton;
class QGroupBox;
class QLineEdit;
class QSpinBox;
class QListWidget;

namespace Dashel
{
	class Stream;
}

namespace Aseba
{
	/** \addtogroup studio */
	/*@{*/
	
	class DashelConnectionDialog : public QDialog
	{
		Q_OBJECT
		
	protected:
		QPushButton* connectButton;
		QGroupBox* netGroupBox;
		QLineEdit* host;
		QSpinBox* port;
		QGroupBox* serialGroupBox;
		QListWidget* serial;
		QGroupBox* customGroupBox;
		QLineEdit* custom;
		
	public:
		DashelConnectionDialog();
		std::string getTarget();
		
	public slots:
		void netGroupChecked();
		void serialGroupChecked();
		void customGroupChecked();
		void setupOkStateFromListSelection();
	};
	
	class Message;	
	
	class DashelTarget: public Target, public Dashel::Hub
	{
		Q_OBJECT
		
	protected:
		struct Node
		{
			Node();
			
			QString name; //!< name of node
			TargetDescription description; //!< description of node
			int localEventsReceptionCounter; //!< what is the status of the reception of local events
			int nativeFunctionReceptionCounter; //!< what is the status of the reception of native functions
			BytecodeVector debugBytecode; //!< bytecode with debug information
			unsigned steppingInNext; //!< state of node when in next and stepping
			unsigned lineInNext; //!< line of node to execute when in next and stepping
			ExecutionMode executionMode; //!< last known execution mode if this node
		};
		
		typedef void (DashelTarget::*MessageHandler)(Message *message);
		typedef std::map<unsigned, MessageHandler> MessagesHandlersMap;
		typedef std::map<unsigned, Node> NodesMap;
		
		MessagesHandlersMap messagesHandlersMap;
		NodesMap nodes;
		Dashel::Stream* stream;
		int netTimer;
		bool quitting;
		
	public:
		DashelTarget();
		~DashelTarget();
		
		virtual void disconnect();
		
		virtual const TargetDescription * const getConstDescription(unsigned node) const;
		
		virtual void uploadBytecode(unsigned node, const BytecodeVector &bytecode);
		virtual void writeBytecode(unsigned node);
		
		virtual void sendEvent(unsigned id, const VariablesDataVector &data);
		
		virtual void setVariables(unsigned node, unsigned start, const VariablesDataVector &data);
		virtual void getVariables(unsigned node, unsigned start, unsigned length);
		
		virtual void reset(unsigned node);
		virtual void run(unsigned node);
		virtual void pause(unsigned node);
		virtual void next(unsigned node);
		virtual void stop(unsigned node);
		
		virtual void setBreakpoint(unsigned node, unsigned line);
		virtual void clearBreakpoint(unsigned node, unsigned line);
		virtual void clearBreakpoints(unsigned node);
		
	protected:
		virtual void timerEvent(QTimerEvent *event);
		virtual void incomingData(Dashel::Stream *stream);
		virtual void connectionClosed(Dashel::Stream *stream, bool abnormal);
		
	protected:
		void receivedDescription(Message *message);
		void receivedLocalEventDescription(Message *message);
		void receivedNativeFunctionDescription(Message *message);
		void receivedDisconnected(Message *message);
		void receivedVariables(Message *message);
		void receivedArrayAccessOutOfBounds(Message *message);
		void receivedDivisionByZero(Message *message);
		void receivedEventExecutionKilled(Message *message);
		void receivedNodeSpecificError(Message *message);
		void receivedExecutionStateChanged(Message *message);
		void receivedBreakpointSetResult(Message *message);
		
	protected:
		bool emitNodeConnectedIfDescriptionComplete(unsigned id, const Node& node);
		int getPCFromLine(unsigned node, unsigned line);
		int getLineFromPC(unsigned node, unsigned pc);
	};
	
	/*@}*/
}; // Aseba

#endif
