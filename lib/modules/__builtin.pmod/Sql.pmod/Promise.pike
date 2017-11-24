#pike __REAL_VERSION__

//#define SP_DEBUG	1
//#define SP_DEBUGMORE	1

#ifdef SP_DEBUG
#define PD(X ...)            werror(X)
                             // PT() puts this in the backtrace
#define PT(X ...)            (lambda(object _this){(X);}(this))
#else
#undef SP_DEBUGMORE
#define PD(X ...)            0
#define PT(X ...)            (X)
#endif

inherit Concurrent.Promise;

private .FutureResult res;
private int minresults, maxresults, discardover;

private void failed(mixed msg) {
  res->_dblink = 0;				// Release reference
  res->status_command_complete = arrayp(msg) ? msg : ({msg, backtrace()});
  PD("Future failed %O %s\n",
   res->query, describe_backtrace(res->status_command_complete));
  failure(res);
}

private void succeeded(.Result result) {
  PD("Future succeeded %O\n", res->query);
  if (discardover >= 0)
    res->raw_data = res->raw_data[.. discardover];
  mixed err =
    catch {
      res->fields = result->fetch_fields();
      res->affected_rows = result->affected_rows();
      res->status_command_complete = result->status_command_complete();
    };
  if (err)
    failed(err);
  else
    success(res);
}

private void result_cb(.Result result, array(array(mixed)) rows) {
  PD("Callback got %O\n", rows);
  if (rows) {
    if (maxresults >= 0 && result->num_rows() > maxresults)
      failed(sprintf("Too many records returned: %d > %d",
                          result->num_rows(), maxresults));
    if (discardover >= 0) {
      int room = discardover - sizeof(res->raw_data);
      if (room >= 0)
        res->raw_data += rows[.. room - 1];
    } else
      res->raw_data += rows;
  } else if (sizeof(res->raw_data) >= minresults)
    succeeded(result);
  else
    failed("Insufficient number of records returned");
}

//! @param min
//!   If the query returns less than this number of records, fail the
//!   future.
final this_program min_records(int min) {
  minresults = min;
  return this;
}

//! @param max
//!   If the query returns more than this number of records, fail the
//!   future.
final this_program max_records(int max) {
  maxresults = max;
  return this;
}

//! @param over
//!   Discard any records over this number.
final this_program discard_records(int over) {
  discardover = over;
  return this;
}

protected
 void create(.Connection db, string q, mapping(string:mixed) bindings) {
  PD("Create future %O %O %O\n", db, q, bindings);
  res = .FutureResult(db, q, bindings);
  discardover = maxresults = -1;
  if (res->status_command_complete = catch(db->big_typed_query(q, bindings)
                                    ->set_result_array_callback(result_cb)))
    failed(res->status_command_complete);
  ::create();
}

#ifdef SP_DEBUG
protected void _destruct() {
  PD("Destroy promise %O %O\n", query, bindings);
  ::_destruct();
}
#endif
