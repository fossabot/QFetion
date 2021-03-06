#include "logintossiportal.h"
#include <QHttpResponseHeader>

LoginToSsiPortal::LoginToSsiPortal( QObject * parent ) :
	QHttp(parent)
{
	this->parent = parent;
	buffer = new QBuffer();
	timer = new QTimer;
	init();

	connect(this, SIGNAL(requestFinished(int, bool)), this, SLOT(handle_request(int, bool)));
	connect(this, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)),
	this, SLOT(handle_header(const QHttpResponseHeader &)));
	connect(this, SIGNAL(sslErrors(const QList<QSslError> &) ),this, SLOT(ignoreSslErrors()));
	connect(timer, SIGNAL(timeout()), this, SLOT(handle_timeout()));
	qDebug() << "LoginToSsiPortal Class loaded";
}

LoginToSsiPortal::~LoginToSsiPortal()
{
	qDebug() << "LoginToSsiPortal Class unloaded";
}

void LoginToSsiPortal::doLogin()
{
	init();
	step = 1;
	buffer->open(QBuffer::ReadWrite);

	this->setHost(syscfg->getSsiPortalHost(), QHttp::ConnectionModeHttps,
			syscfg->getSsiPortalPort());
	//	this->head();
	timer->start(25000);
	httpGetId = this->get(syscfg->getSsiPortalPath().arg(acount_name).arg(
			acount_pass), buffer);
	addtoLog(tr("sent login request"));
	return;
}

void LoginToSsiPortal::toGetProxyHost( const QString &mob )
{
	init();
	step = 2;
	qDebug() << "LoginToSsiPortal : " << mob;
	buffer->open(QBuffer::ReadWrite);
	this->setHost(syscfg->getSsiPortalHost(), QHttp::ConnectionModeHttp, 80);
	//	this->head();
	timer->start(25000);
	httpGetId = this->post("/nav/getsystemconfig.aspx", tr(
			"<config><user mobile-no=\"%1\" />"
				"<client type=\"PC\" version=\"4.0.2510\" platform=\"W5.1\" />"
				"<servers version=\"0\" />"
				//										"<service-no version=\"0\" />"
				//										"<parameters version=\"0\" />"
				//										"<hints version=\"0\" />"
				"<http-applications version=\"0\" />"
				"</config>").arg(mob).toAscii(), buffer);
	addtoLog(tr("to get proxy host"));
	return;
}

void LoginToSsiPortal::init()
{
	aborted = false;
	httpGetId = 0;
	//	buffer->setData("");
	if( buffer->isOpen() ) buffer->close();
	error_str = "";
	timer->stop();
}

void LoginToSsiPortal::setAcountName( const QString &name )
{
	this->acount_name = name;
}

void LoginToSsiPortal::setAcountPass( const QString &pass )
{
	this->acount_pass = pass;
}

void LoginToSsiPortal::setSysConfig( SysConfig *config )
{
	this->syscfg = config;
}

void LoginToSsiPortal::handle_request( int requestId, bool error )
{
	addtoLog(tr("Got Response"));

	if( step == 1 )
	{
		if( aborted )
		{
			finished1(true, "", "Aborted");
			return;
		}
		if( requestId != httpGetId )
		{// It is OK
			return;
		}
		timer->stop();
		if( error )
		{
			error_str = this->errorString();
			addtoLog(tr("login failed: %1").arg(error_str));
			finished1(true, "", error_str);
			return;
		}
		finished1(false, get_uri(buffer->buffer().data()));
	}
	if( step == 2 )
	{
		if( aborted )
		{
			got_proxyinfo(true, "", "Aborted");
			return;
		}
		if( requestId != httpGetId )
		{// It is OK
			return;
		}
		timer->stop();
		if( error )
		{
			error_str = this->errorString();
			addtoLog(tr("login failed: %1").arg(error_str));
			got_proxyinfo(true, "", error_str);
			return;
		}
		get_proxyinfo(buffer->buffer().data());
	}
	timer->stop();
}

void LoginToSsiPortal::get_proxyinfo( const QString &res )
{
	qDebug() << res;
	/*"<?xml version="1.0" encoding="utf-8" ?><config><servers version="76">
	 * <sipc-proxy>221.130.45.203:8080</sipc-proxy>
	 * <sipc-ssl-proxy>221.130.45.203:443</sipc-ssl-proxy>
	 * <http-tunnel>HTTP://221.130.45.203/ht/sd.aspx</http-tunnel>
	 * <ssi-app-sign-in>https://221.130.45.201/ssiportal/SSIAppSignIn.aspx</ssi-app-sign-in>
	 * <ssi-app-sign-out>http://221.130.45.201/ssiportal/SSIAppSignOut.aspx</ssi-app-sign-out>
	 * <stun-urls>stunserver.org:3478</stun-urls>
	 * <sub-service>https://221.130.45.201/nav/Subscribe.aspx</sub-service>
	 * <apply-sub-service>HTTP://221.130.45.201/nav/ApplySubscribe.aspx</apply-sub-service>
	 * <crbt-portal>http://221.130.46.134/crbt/default.aspx</crbt-portal>
	 * <email-adapter>http://221.130.45.215/EmailAdapter/</email-adapter>
	 * <get-general-info>HTTP://221.130.45.201/nav/GeneralGetInfo.aspx</get-general-info>
	 * <get-pic-code>HTTP://221.130.45.201/nav/GetPicCode.aspx</get-pic-code>
	 * <get-svc-order-status>HTTP://221.130.45.201/nav/GetSvcOrderStatus.aspx</get-svc-order-status>
	 * <get-system-status>HTTP://221.130.45.201/nav/GetSystemStatus.aspx</get-system-status>
	 * <get-uri>HTTP://221.130.45.205/hds/geturi.aspx</get-uri>
	 */
	qDebug() << res;
	QDomDocument doc;
	if( !doc.setContent(res) )
	{
		got_proxyinfo(true, "", "cannot open response as XML file");
		return;
	}
	QDomElement root = doc.documentElement();
	if( (root.isNull()) || (root.tagName() != "config") )
	{
		got_proxyinfo(true, "", "wrong response format (no config elem)");
		return;
	}
	QDomElement userElem = root.firstChildElement("servers").firstChildElement(
			"sipc-proxy");
	QString tmp;
	if( !userElem.isNull() )
	{
		tmp = userElem.text();
	}
	got_proxyinfo(false, "sipc-proxy", tmp.section(":", 0, 0));
	userElem = root.firstChildElement("http-applications").firstChildElement(
			"get-portrait");
	if( !userElem.isNull() )
	{
		tmp = userElem.text();
	}
	got_proxyinfo(false, "get-portrait", tmp);
}

QString LoginToSsiPortal::get_uri( const QString &res )
{
	//TODO  to add XML parse code for uri
	//response is like below:
	//	<?xml version="1.0" encoding="utf-8" ?>
	//		<results status-code="200">
	//			<user uri="sip:xxxxxxxxx@fetion.com.cn;p=xxxx" mobile-no="xxxxxxxxx" user-status="101">
	//				<credentials>
	//				</credentials>
	//			</user>
	//		</results>
	QDomDocument doc;
	if( !doc.setContent(res) )
	{
		finished1(true, "", "cannot open response as XML file");
		return "";
	}
	QDomElement root = doc.documentElement();
	if( (root.isNull()) || (root.tagName() != "results")
			|| (!root.hasAttribute("status-code")) )
	{
		finished1(true, "", "wrong response format (no result elem)");
		return "";
	}
	if( root.attribute("status-code") != "200" )
	{
		finished1(true, "", "wrong acount name or password");
		return "";
	}
	QDomElement userElem = root.firstChildElement("user");
	if( (userElem.isNull()) || (userElem.tagName() != "user")
			|| (!userElem.hasAttribute("uri")) || (!userElem.hasAttribute(
			"mobile-no")) )
	{
		finished1(true, "",
				"wrong response format (no user elem in result elem)");
		return "";
	}
	QString uri = userElem.attribute("uri");
	if( (!uri.contains("@fetion.com.cn;p=")) || !uri.startsWith("sip:",
			Qt::CaseInsensitive) )
	{
		finished1(true, "", "found uri, but it has wrong format");
		return "";
	}
	return uri;
}

void LoginToSsiPortal::handle_header( const QHttpResponseHeader &header )
{
	addtoLog(tr("Got Header"));
	QString tmp;
	switch( header.statusCode() )
	{
		case 200: // Ok
			//	case 301: // Moved Permanently
			//	case 302: // Found
			//	case 303: // See Other
			//	case 307: // Temporary Redirect
			// these are not error conditions
			if( header.hasKey("Set-Cookie") && (step == 1) )
			{
				tmp = header.value("Set-Cookie").section(";", 0, 0);
				if( tmp.startsWith("ssic=") )
				{
					tmp.remove(0, 5);
					qDebug() << "ssic" << tmp;
					got_ssic(tmp);
				}
			}

			break;

		default:
			addtoLog(tr("login failed:  %1").arg(header.reasonPhrase()));
			this->abort();
			aborted = true;
			if( step == 1 ) finished1(true, "", QString::number(
					header.statusCode()));
			if( step == 2 ) got_proxyinfo(true, "", QString::number(
					header.statusCode()));
	}
	return;
}

void LoginToSsiPortal::handle_sslerrors( const QList<QSslError> & errors )
{
	QSslError error;
	foreach(error,errors)
		{
			addtoLog(error.errorString());
		}
}

void LoginToSsiPortal::handle_timeout()
{
	timer->stop();
	if( step == 1 ) finished1(true, "", "timeout");
	if( step == 2 ) got_proxyinfo(true, "", "timeout");
}
