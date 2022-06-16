/*  smplayer, GUI front-end for mplayer.
    Copyright (C) 2006-2021 Ricardo Villalba <ricardo@smplayer.info>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


/***********************************************************************
 * Copyright 2012  Eike Hein <hein@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************/

#include <QMetaObject>
#include <locale.h>
#include "httpcontrol.h"
#include "basegui.h"
#include "core.h"
#include "playlist.h"
#include "EmbeddableWebServer.h"

HTTPControl *HTTPControlObject;
static uint16_t HTTPControlPort;

static THREAD_RETURN_TYPE STDCALL_ON_WIN32 acceptConnectionsThread(void* port) {
    acceptConnectionsUntilStoppedFromEverywhereIPv4(NULL, HTTPControlPort);
    return (THREAD_RETURN_TYPE) 0;
}

struct Response* createResponseForRequest(const struct Request* request, struct Connection* connection) {
	return HTTPControlObject->createResponseForRequest(request, connection);
}

HTTPControl::HTTPControl(BaseGui* gui, uint16_t port, QObject* parent)
    : m_core(gui->getCore()),
      m_playlist(gui->getPlaylist())
{
	HTTPControlObject = this;
	HTTPControlPort = port;
    pthread_t threadHandle;
    pthread_create(&threadHandle, NULL, &acceptConnectionsThread, NULL);
}

HTTPControl::~HTTPControl() {
}

void HTTPControl::open(QString name)
{
	m_core->open(name);
}

struct Response* HTTPControl::createResponseForRequest(const struct Request* request, struct Connection* connection)
{
    locale_t c = newlocale (LC_MESSAGES_MASK, "C", (locale_t)0);
    uselocale (c);
    
    if(strcmp(request->method,"POST")==0) {
		char *cmd = strdupDecodePOSTParam("command=", request, "X");
		if(*cmd != 'X') {
			if(strcmp(cmd,"play")==0) m_core->play();
			else if(strcmp(cmd,"playpause")==0) m_core->play_or_pause();
			else if(strcmp(cmd,"pause")==0) m_core->setPause(true);
			else if(strcmp(cmd,"stop")==0) m_core->stop();
			else if(strcmp(cmd,"next")==0) m_playlist->playNext();
			else if(strcmp(cmd,"prev")==0) m_playlist->playPrev();
			else if(strcmp(cmd,"seek")==0) {
				char *val = strdupDecodePOSTParam("value=", request, "X");
				double v = atof(val);
				if(v!=0.0) m_core->seek(static_cast<int>(v));
			}
		}
    
		char *filename = strdupDecodePOSTParam("filename=", request, "none");
		if(strcmp(filename,"none")!=0) {
			fprintf(stderr, "Filename: %s\n", filename);
			
			QMetaObject::invokeMethod(HTTPControlObject, "open", Qt::QueuedConnection, QGenericReturnArgument(), Q_ARG(QString, QString::fromUtf8(filename)));

		}
		char *pos = strdupDecodePOSTParam("position=", request, "NaN");
		if(strcmp(pos,"NaN")!=0) {
			double p = atof(pos);
			if(p>=0.0) m_core->goToSec(p);
		}
		char *rate = strdupDecodePOSTParam("rate=", request, "NaN");
		if(strcmp(rate,"NaN")!=0) {
			double r = atof(rate);
			if(r>=0.01 && r<=100.0) m_core->setSpeed(r);
		}
		char *vol = strdupDecodePOSTParam("volume=", request, "NaN");
		if(strcmp(vol,"NaN")!=0) {
			double v = atof(vol)*100.0;
			if(v<=0.0) m_core->setVolume(0);
			else if(v>=100.0) m_core->setVolume(100);
			else m_core->setVolume(static_cast<int>(v));
		}
	}
	
	return responseAllocJSONWithFormat("{ \"state\": \"%s\", "
	                                     "\"filename\": \"%s\", "
	                                     "\"position\": %#f, "
	                                     "\"rate\": %#f, "
	                                     "\"volume\": %#f }",
	                                     m_core->stateToString().toLocal8Bit().constData(),
	                                     m_core->mdat.filename.toLocal8Bit().constData(),
	                                     m_core->mset.current_sec,
	                                     m_core->mset.speed,
	                                     m_core->currentVolume() / 100.0);
}

