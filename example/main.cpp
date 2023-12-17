// lib.cpp : Defines the entry point for the application.
//
#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>
#include <asiopq/Connection.hpp>
#include <iostream>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

asio::experimental::coro<void> DBConnect(::asio::any_io_executor& executor)
{
	using PC::asiopq::ResultPtr;
	std::string connection_string{"postgresql://postgres:postgres@localhost:5432/postgres"};
	{
		PC::asiopq::Connection connection;
		co_await connection.connect_async(executor, connection_string);
		if (connection.status() != ConnStatusType::CONNECTION_OK)
			co_return;
		{
			auto init_reses = connection.commands_async(executor, "DROP TABLE IF EXISTS TBL1;DROP TABLE IF EXISTS TBL2; CREATE TABLE IF NOT EXISTS TBL1 (i int4);CREATE TABLE IF NOT EXISTS TBL2 (i int4);");
			while (auto res = co_await init_reses)
			{
				if(not res.has_value())
				{
					continue;
				}
				if (res->status() != PGRES_COMMAND_OK)
				{
					std::cout << res->status() << " : " << res->error_msg() << "\n";
					co_return;
				}
			}
			std::cout << "Create was a success\n";
		}
		{
			auto const insert_res = co_await connection.command_async(executor, "INSERT INTO TBL1 VALUES(25)");
			std::cout << static_cast<bool>(insert_res) << " => " << insert_res.status() << " : " << connection.error_msg() << " : " << insert_res.error_msg() << "\n";
			if (not insert_res or insert_res.status() != PGRES_COMMAND_OK)
				co_return;
			std::cout << "Insert was a success\n";
		}
		{
			auto const result_set = co_await connection.command_async(executor, "SELECT * from TBL1");
			if (not result_set)
			{
				::std::cerr << "Select from Result set failed " << result_set.error_msg() << ::std::endl;
				co_return;
			}
			if (result_set.status() != PGRES_TUPLES_OK)
				co_return;
			auto const row_count = result_set.row_count();
			std::cout << "Select table Row Count" << row_count << "\n";
			for (int row = 0; row < row_count; ++row)
			{
				std::cout << "Row " << row << " : " << result_set(0, row).as<int>() << "\n";
			}
		}
		{
			auto notifies = connection.await_notify_async(executor, "LISTEN TBL1;");
			while (auto notify_item = co_await notifies)
			{
				std::cout << "Notify Rel " << (*notify_item)->relname << " " << (*notify_item)->extra << "\n";
			}
		}
	}
	{
		PC::asiopq::Connection connection;
		connection.connect(connection_string);
		std::cout << std::boolalpha << "Success " << connection.error_msg() << (connection.status() == ConnStatusType::CONNECTION_OK) << "\n";
	}
	co_return;
}

asio::awaitable<void> DBConnect()
{
	auto executor = co_await ::asio::this_coro::executor;
	auto op = DBConnect(executor);
	co_return co_await op.async_resume(::asio::use_awaitable);
}


int main()
{
	asio::io_context io_context;
	asio::co_spawn(io_context, DBConnect(), asio::detached);
	io_context.run();
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