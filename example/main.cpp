// lib.cpp : Defines the entry point for the application.
//
#include <asiopq/Connection.hpp>
#include <iostream>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

asio::awaitable<void> DBConnect()
{
	using PC::asiopq::ResultPtr;
	std::string connection_string{"postgresql://postgres:postgres@localhost:5432/postgres"};
	{
		PC::asiopq::Connection connection;
		co_await connection.connect_async(connection_string);
		if (connection.status() != ConnStatusType::CONNECTION_OK)
			co_return;
		{
			auto const init_reses = co_await connection.commands_async("DROP TABLE IF EXISTS TBL1;DROP TABLE IF EXISTS TBL2; CREATE TABLE IF NOT EXISTS TBL1 (i int4);CREATE TABLE IF NOT EXISTS TBL2 (i int4);");
			if (std::empty(init_reses))
				co_return;

			for (auto const &res : init_reses)
			{
				if (res.status() != PGRES_COMMAND_OK)
				{
					std::cout << res.status() << " : " << res.error_msg() << "\n";
					co_return;
				}
			}
			std::cout << "Create was a success\n";
		}
		{
			auto const insert_res = co_await connection.command_async("INSERT INTO TBL1 VALUES(25)");
			std::cout << ((bool)insert_res) << insert_res.status() << " : " << connection.error_msg() << " : " << insert_res.error_msg() << "\n";
			if (not insert_res or insert_res.status() != PGRES_COMMAND_OK)
				co_return;
			std::cout << "Insert was a success\n";
		}
		{
			auto const result_set = co_await connection.command_async("SELECT * from TBL1");
			if (not result_set)
				co_return;
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
			co_await connection.command_async("LISTEN TBL1;");
			auto executor = co_await asio::this_coro::executor;
			auto notifies = connection.await_notify_async(executor);
			for (std::size_t i = 0; i < 50; ++i)
			{
				auto notify_item = ((co_await notifies.async_resume(asio::use_awaitable)).value());
				std::cout << "Notify Rel " << notify_item->relname << " " << notify_item->extra << "\n";
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