#include <asiopq/Connection.hpp>
#include <asiopq/Exception.hpp>
#include <asiopq/Pipeline.hpp>
#include <asiopq/ResultPtr.hpp>
#include <sstream>
#include <string>

namespace PC::asiopq
{
   ::std::string GenerateMessage(Connection const& conn)
   {
      using Stream = ::std::ostringstream;
      Stream stream;
      stream << "Error Connection(Status=" << conn.status() << " MSG=" << conn.error_msg()
             << ")";
      return stream.str();
   }
   ::std::string GenerateMessage(Connection const& conn, ResultPtr const& result)
   {
      using Stream = ::std::ostringstream;
      auto   error = GenerateMessage(conn);
      Stream stream;
      stream << error << " Result(Status=" << result.status()
             << " MSG=" << result.error_msg() << ")";
      return stream.str();
   }
   Exception::Exception(Connection const& conn) :
       ::std::runtime_error(GenerateMessage(conn))
   {
   }
   Exception::Exception(Pipeline const& pipeline) : Exception(pipeline.conn()) {}
   Exception::Exception(Connection const& conn, ResultPtr const& result) :
       ::std::runtime_error(GenerateMessage(conn, result))
   {
   }
   Exception::Exception(Pipeline const& pipeline, ResultPtr const& result) :
       Exception(pipeline.conn(), result)
   {
   }
} // namespace PC::asiopq