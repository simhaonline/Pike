/*
**! module Protocols
**! submodule HTTP
**! class Query
**!
**! 	Open and execute a HTTP query.
**!
**! method object thread_request(string server,int port,string query);
**! method object thread_request(string server,int port,string query,mapping headers,void|string data);
**!	Create a new query object and begin the query.
**!	
**!	The query is executed in a background thread;
**!	call '() in this object to wait for the request
**!	to complete.
**!
**!	'query' is the first line sent to the HTTP server;
**!	for instance "GET /index.html HTTP/1.1".
**!
**!	headers will be encoded and sent after the first line,
**!	and data will be sent after the headers.
**!
**! returns the called object
**!
**! method object set_callbacks(function request_ok,function request_fail,mixed ...extra)
**! method object async_request(string server,int port,string query);
**! method object async_request(string server,int port,string query,mapping headers,void|string data);
**!	Setup and run an asynchronous request,
**!	otherwise similar to thread_request.
**!
**!	request_ok(object httpquery,...extra args)
**!	will be called when connection is complete,
**!	and headers are parsed.
**!
**!	request_fail(object httpquery,...extra args)
**!	is called if the connection fails.
**!
**! returns the called object
**!
**! variable int ok
**!	Tells if the connection is successfull.
**! variable int errno
**!	Errno copied from the connection.
**!
**! variable mapping headers
**!	Headers as a mapping.
**!
**! variable string protocol
**!	Protocol string, ie "HTTP/1.0".
**!
**! variable int status
**! variable string status_desc
**!	Status number and description (ie, 200 and "ok").
**!
**! variable mapping hostname_cache
**!	Set this to a global mapping if you want to use a cache,
**!	prior of calling *request().
**!
**! variable mapping async_dns
**!	Set this to an array of Protocols.DNS.async_clients,
**!	if you wish to limit the number of outstanding DNS
**!	requests. Example:
**!	   async_dns=allocate(20,Protocols.DNS.async_client)();
**!
**! method int `()()
**!	Wait for connection to complete.
**! returns 1 on successfull connection, 0 if failed
**!
**! method array cast("array")
**!	Gives back ({mapping headers,string data,
**!		     string protocol,int status,string status_desc});
**!
**! method mapping cast("mapping")
**!	Gives back 
**!	headers | 
**!	(["protocol":protocol,
**!	  "status":status number,
**!	  "status_desc":status description,
**!	  "data":data]);
**!		
**! method string cast("string")
**!	Gives back the answer as a string.
**!
**! method string data()
**!	Gives back the data as a string.
**!
**! object(pseudofile) file()
**! object(pseudofile) file(mapping newheaders,void|mapping removeheaders)
**! object(pseudofile) datafile();
**!	Gives back a pseudo-file object,
**!	with the method read() and close().
**!	This could be used to copy the file to disc at
**!	a proper tempo.
**!
**!	datafile() doesn't give the complete request, 
**!	just the data.
**!
**!	newheaders, removeheaders is applied as:
**!	<tt>(oldheaders|newheaders))-removeheaders</tt>
**!	Make sure all new and remove-header indices are lower case.
**!
**! void async_fetch(function done_callback);
**!	Fetch all data in background.
*/

#define error(S) throw( ({(S),backtrace()}) )

/****** variables **************************************************/

// open

int errno;
int ok;

mapping headers;

string protocol;
int status;
string status_desc;

int timeout=120; // seconds

// internal 

object con;
string request;

string buf="",headerbuf="";
int datapos;
object conthread;

function request_ok,request_fail;
array extra_args;

/****** internal stuff *********************************************/

static void ponder_answer()
{
   // read until we have all headers

   int i=0;

   for (;;)
   {
      string s;

      if (i<0) i=0;
      if ((i=search(buf,"\r\n\r\n",i))!=-1) break;

      s=con->read(8192,1);
      if (s=="") { i=strlen(buf); break; }
      
      i=strlen(buf)-3;
      buf+=s;
   }

   headerbuf=buf[..i-1];
   datapos=i+4;

   // split headers

   headers=([]);
   sscanf(headerbuf,"%s%*[ ]%d%*[ ]%s%*[\r\n]",protocol,status,status_desc);
   foreach ((headerbuf/"\r\n")[1..],string s)
   {
      string n,d;
      sscanf(s,"%[!-9;-~]%*[ \t]:%*[ \t]%s",n,d);
      headers[lower_case(n)]=d;
   }

   // done
   ok=1;
   remove_call_out(async_timeout);

   if (request_ok) request_ok(this_object(),@extra_args);
}

static void connect(string server,int port)
{
   if (catch { con->connect(server,port); })
   {
      if (!(errno=con->errno())) errno=22; /* EINVAL */
      destruct(con);
      con=0;
      ok=0;
      return;
   }

   con->write(request);

   ponder_answer();
}

static void async_close()
{
   con->set_blocking();
   ponder_answer();
}

static void async_read(mixed dummy,string s)
{
   buf+=s;
   if (-1!=search(buf,"\r\n\r\n")) 
   {
      con->set_blocking();
      ponder_answer();
   }
}

static void async_write()
{
   con->set_blocking();
   con->write(request);
   con->set_nonblocking(async_read,0,async_close);
}

static void async_connected()
{
   con->set_nonblocking(async_read,async_write,async_close);
   con->write("");
}

static void async_failed()
{
   if (con) errno=con->errno; else errno=113; // EHOSTUNREACH
   ok=0;
   if (request_fail) request_fail(this_object(),@extra_args);
   remove_call_out(async_timeout);
}

static void async_timeout()
{
   errno=110; // timeout
   if (con)
   {
      catch { con->close(); };
      destruct(con); 
   }
   con=0;
   async_failed();
}

void async_got_host(string server,int port)
{
   if (!server)
   {
      async_failed();
      catch { destruct(con); }; //  we may be destructed here
      return;
   }
   if (catch
   {
      con=Stdio.File();
      if (!con->open_socket())
	 error("HTTP.Query(): can't open socket; "+strerror(con->errno)+"\n");
   }) 
   {
      return; // got timeout already, we're destructed
   }

   con->set_nonblocking(0,async_connected,async_failed);
//    werror(server+"\n");

   if (catch { con->connect(server,port); })
   {
      if (!(errno=con->errno())) errno=22; /* EINVAL */
      destruct(con);
      con=0;
      ok=0;
      async_failed();
   }
}

void async_fetch_read(mixed dummy,string data)
{
   buf+=data;
}

void async_fetch_close()
{
   con->set_blocking();
   destruct(con);
   con=0;
   request_ok(@extra_args);
}

/****** utilities **************************************************/

string headers_encode(mapping h)
{
   if (!h || !sizeof(h)) return "";
   return Array.map( indices(h),
		     lambda(string hname,mapping headers)
		     {
			if (stringp(headers[hname]))
			   return String.capitalize(hname) + 
			      ": " + headers[hname];
		     }, h )*"\r\n" + "\r\n";
}

/****** helper methods *********************************************/

mapping hostname_cache=([]);
array async_dns=0;

void dns_lookup_callback(string name,string ip,function callback,
			 mixed ...extra)
{
   hostname_cache[name]=ip;
   if (functionp(callback))
      callback(ip,@extra);
}

void dns_lookup_async(string hostname,function callback,mixed ...extra)
{
   string id;
   if (hostname=="")
   {
      call_out(callback,0,0,@extra); // can't lookup that...
      return;
   }
   sscanf(hostname,"%*[0-9.]",id);
   if (hostname==id ||
       (id=hostname_cache[hostname]))
   {
      call_out(callback,0,id,@extra);
      return;
   }

   if (!async_dns)
      Protocols.DNS.async_client()
	 ->host_to_ip(hostname,dns_lookup_callback,callback,@extra);
   else
      async_dns[random(sizeof(async_dns))]->
	 host_to_ip(hostname,dns_lookup_callback,callback,@extra);
}

string dns_lookup(string hostname)
{
   string id;
   sscanf(hostname,"%*[0-9.]",id);
   if (hostname==id ||
       (id=hostname_cache[hostname]))
      return id;

   array hosts=((Protocols.DNS.client()->gethostbyname(hostname)||
		 ({0,0}))[1]+({0}));
   return hosts[random(sizeof(hosts))];
}


/****** called methods *********************************************/

object set_callbacks(function(object,mixed...:mixed) _ok,
		     function(object,mixed...:mixed) _fail,
		     mixed ...extra)
{
   extra_args=extra;
   request_ok=_ok;
   request_fail=_fail;
   return this_object();
}

object thread_request(string server,int port,string query,
		      void|mapping|string headers,void|string data)
{
   // start open the connection
   
   con=Stdio.File();
   if (!con->open_socket())
      error("HTTP.Query(): can't open socket; "+strerror(con->errno)+"\n");

   string server1=dns_lookup(server);

   if (server1) server=server1; // cheaty, if host doesn't exist

   conthread=thread_create(connect,server,port);

   // prepare the request

   if (!data) data="";

   if (!headers) headers="";
   else if (mappingp(headers))
   {
      headers=mkmapping(Array.map(indices(headers),lower_case),
			values(headers));

      if (data!="") headers->content_length=strlen(data);

      headers=headers_encode(headers);
   }
   
   request=query+"\r\n"+headers+"\r\n"+data;

   return this_object();
}

object async_request(string server,int port,string query,
		     void|mapping|string headers,void|string data)
{
   // start open the connection
   
   call_out(async_timeout,timeout);

   dns_lookup_async(server,async_got_host,port);

   // prepare the request

   if (!data) data="";

   if (!headers) headers="";
   else if (mappingp(headers))
   {
      headers=mkmapping(Array.map(indices(headers),lower_case),
			values(headers));

      if (data!="") headers->content_length=strlen(data);

      headers=headers_encode(headers);
   }
   
   request=query+"\r\n"+headers+"\r\n"+data;
   
   return this_object();
}

int `()()
{
   // wait for completion
   if (conthread) conthread->wait();
   return ok;
}

string data()
{
   `()();
   int len=(int)headers["content-length"];
   int l;
   if (zero_type(len)) 
      l=0x7fffffff;
   else
      l=len-strlen(buf)+4+strlen(headerbuf);
   if (l>0 && con)
   {
      string s=con->read(l);
      buf+=s;
   }
   // note! this can't handle keep-alive � HEAD requests.
   return buf[datapos..];
}

array|mapping|string cast(string to)
{
   switch (to)
   {
      case "mapping":
	 `()();
	 return headers|
	    (["data":data(),
	      "protocol":protocol,
	      "status":status,
	      "status_desc":status_desc]);
      case "array":
	 `()();
	 return ({headers,data(),protocol,status,status_desc});
      case "string":
	 `()();
         data();
         return buf;
   }
   error("HTTP.Query: can't cast to "+to+"\n");
}

class PseudoFile
{
   string buf;
   object con;
   int len;
   int p=0;

   void create(object _con,string _buf,int _len)
   {
      con=_con;
      buf=_buf;
      len=_len;
      if (!con) len=strlen(buf);
   }

   string read(int n)
   {
      string s;
      
      if (p+n>len) n=len-p;

      if (strlen(buf)<n && con)
	 buf+=con->read(n-strlen(buf));

      s=buf[..n-1];
      buf=buf[n..];
      p+=strlen(s);
      return s;
   }

   void close()
   {
      catch { con->close(); destruct(con); };
      con=0; // forget
   }
};

object file(void|mapping newheader,void|mapping removeheader)
{
   `()();
   mapping h=headers;
   int len;
   if (newheader||removeheader)
   {
      h=(h|(newheader||([])))-(removeheader||([]));
      string hbuf=headers_encode(h);
      if (hbuf=="") hbuf="\r\n";
      if (zero_type(headers["content-length"]))
	 len=0x7fffffff;
      else 
	 len=strlen(protocol+" "+status+" "+status_desc)+2+
	    strlen(hbuf)+2+(int)headers["content-length"];
      return PseudoFile(con,
			protocol+" "+status+" "+status_desc+"\r\n"+
			hbuf+"\r\n"+buf[datapos..],len);
   }
   if (zero_type(headers["content-length"]))
      len=0x7fffffff;
   else 
      strlen(headerbuf)+4+(int)h["content-length"];
   return PseudoFile(con,buf,len);
}

object datafile()
{
   `()();
   return PseudoFile(con,buf[datapos..],(int)headers["content-length"]);
}

void destroy()
{
   catch { con->close(); destruct(con); };
}

void async_fetch(function callback,array ... extra)
{
   if (!con)
   {
      callback(@extra); // nothing to do, stupid...
      return; 
   }
   extra_args=extra;
   request_ok=callback;
   con->set_nonblocking(async_fetch_read,0,async_fetch_close);
}

/************************ example *****************************/

#if 0

object o=HTTP.Query();

void ok()
{
   write("ok...\n");
   write(sprintf("%O\n",o->headers));
   exit(0);
}

void fail()
{
   write("fail\n");
   exit(0);
}

int main()
{
   o->set_callbacks(ok,fail);
   o->async_request("www.roxen.com",80,"HEAD / HTTP/1.0");
   return -1;
}

#endif
