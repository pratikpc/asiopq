// lib.cpp : Defines the entry point for the application.
//
#include <asiopq/Connection.hpp>
#include <asiopq/Exception.hpp>
#include <asiopq/Pipeline.hpp>
#include <boost/cobalt/async_for.hpp>
#include <boost/cobalt/join.hpp>
#include <boost/cobalt/run.hpp>
#include <boost/cobalt/task.hpp>
#include <boost/cobalt/this_coro.hpp>
#include <iostream>
#include <string>
#include <vector>

boost::cobalt::task<void> DBConnect(::std::string                connection_string,
                                    ::std::vector<::std::string> listens)
{
   using PC::asiopq::ResultPtr;
   {
      PC::asiopq::Connection connection{};
      co_await connection.connect_async(connection_string);
      {
         auto init_reses = connection.commands_async(
             "DROP TABLE IF EXISTS TBL1;DROP TABLE IF EXISTS TBL2; CREATE TABLE IF NOT "
             "EXISTS TBL1 (i int4);CREATE TABLE IF NOT EXISTS TBL2 (i int4);");
         BOOST_COBALT_FOR(auto res, init_reses)
         {
            if (not res)
            {
               break;
            }
            ::std::cout << "Received result " << res.status() << ::std::endl;
            if (res.status() != PGRES_COMMAND_OK)
            {
               throw ::PC::asiopq::Exception(connection, res);
            }
         }
         std::cout << "Create was a success\n";
      }
      {
         auto const insert_res =
             co_await connection.command_async("INSERT INTO TBL1 VALUES(25)");
         if (not insert_res or insert_res.status() != PGRES_COMMAND_OK)
         {
            throw ::PC::asiopq::Exception(connection, insert_res);
         }
         std::cout << "Insert was a success\n";
      }
      {
         ::PC::asiopq::Pipeline pipeline{connection};
         if (not pipeline.enter())
         {
            throw ::PC::asiopq::Exception{pipeline};
         }
         for (::std::size_t i = 0; i < 20; ++i)
         {
            ::std::string command{"INSERT INTO TBL1 VALUES(18)"};
            if (PQsendQueryParams(pipeline->native_handle(),
                                  std::data(command),
                                  0,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  0) != 1)
            {
               throw ::PC::asiopq::Exception{pipeline};
            }
         }
         auto init_reses = pipeline.async_wait();
         BOOST_COBALT_FOR(auto res, init_reses)
         {
            if (not res)
            {
               break;
            }
            if (res.status() != PGRES_COMMAND_OK)
            {
               throw ::PC::asiopq::Exception{pipeline, res};
            }
         }
         std::cout << "Pipeline Insert was a success\n";
      }
      {
         auto const result_set = co_await connection.command_async("SELECT * from TBL1");
         if (not result_set)
         {
            throw ::PC::asiopq::Exception{connection, result_set};
         }
         if (result_set.status() != PGRES_TUPLES_OK)
         {
            throw ::PC::asiopq::Exception{connection, result_set};
         }
         auto const row_count = result_set.row_count();
         std::cout << "Select table Row Count" << row_count << "\n";
         for (int row = 0; row < row_count; ++row)
         {
            std::cout << "Row " << row << " : " << result_set(0, row).as<int>() << "\n";
         }
      }
      /// @note Stream selection
      {
         auto results_it = connection.stream_async("SELECT * from TBL1");
         BOOST_COBALT_FOR(auto result_set, results_it)
         {
            /// @note This reaches the final step
            if (not result_set)
            {
               break;
            }
            auto const status = result_set.status();
            if (status == PGRES_TUPLES_OK)
               continue;
            if (status == PGRES_SINGLE_TUPLE)
            {
               auto const row_count = result_set.row_count();
               std::cout << "Stream Select table Row Count" << row_count << "\n";
               for (int row = 0; row < row_count; ++row)
               {
                  std::cout << "Stream Row " << row << " : "
                            << result_set(0, row).as<int>() << "\n";
               }
            }
            else
            {
               throw ::PC::asiopq::Exception{connection, result_set};
            }
         }
      }
      /// @note Copy Command
      {
         auto results =
             connection.copy_from_db("COPY (SELECT * from tbl1) TO STDOUT WITH CSV");
         BOOST_COBALT_FOR(auto result, results)
         {
            if (not result)
               continue;
            ::std::cout << "Received from Result Data " << *result << '\n';
         }
         ::std::cout << "Begin waiting for final completion\n" << ::std::flush;
      }
      {
         for (auto const& listen : listens)
         {
            ::std::string command = "LISTEN " + listen;
            co_await connection.command_async(command);
         }
         auto notifies = connection.await_notify_async();
         BOOST_COBALT_FOR(auto notify_item, notifies)
         {
            std::cout << "Notify Rel " << notify_item->relname << " "
                      << notify_item->extra << "\n";
         }
      }
   }
   {
      PC::asiopq::Connection connection{};
      connection.connect(connection_string);
      std::cout << std::boolalpha << "Success " << connection.error_msg()
                << (connection.status() == ConnStatusType::CONNECTION_OK) << "\n";
   }
   co_return;
}

boost::cobalt::task<void> DBConnect()
{
   ::std::vector<::boost::cobalt::task<void>> tasks;
   std::string                                connection_string{
       "postgresql://postgres:postgres@localhost:5432/postgres"};
   tasks.push_back(
       DBConnect(connection_string, ::std::vector<::std::string>{"tbl1", "tbl4"}));
   tasks.push_back(
       DBConnect(connection_string, ::std::vector<::std::string>{"tbl3", "tbl2"}));
   co_return co_await ::boost::cobalt::join(tasks);
}

int main()
{
   boost::cobalt::run(DBConnect());
   return 0;
}

// CREATE OR REPLACE FUNCTION notify_account_changes()
// RETURNS trigger AS $$
// BEGIN
//   PERFORM pg_notify(
//     'tbl1',
//     json_build_object(
//       'record', row_to_json(NEW)
//     )::text
//   );

//   RETURN NEW;
// END;
// $$ LANGUAGE plpgsql;

// CREATE OR REPLACE TRIGGER accounts_changed
// AFTER INSERT OR UPDATE
// ON TBL1
// FOR EACH ROW
// EXECUTE PROCEDURE notify_account_changes();
// insert into tbl1 values (19),(16),(12),(8);