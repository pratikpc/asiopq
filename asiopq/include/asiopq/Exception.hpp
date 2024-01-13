#include <stdexcept>

namespace PC::asiopq
{
   /// @note Forward declaring because it is only to be used as a reference here
   struct Connection;
   struct ResultPtr;
   struct Pipeline;

   /// @brief Indicates an error with the database connection
   /// @note The exception error message would also contain the required error messages
   ///      This is for making things easier for debugging purposes
   struct Exception : public ::std::runtime_error
   {
    public:
      explicit Exception(Connection const& conn);
      explicit Exception(Pipeline const& pipeline);
      explicit Exception(Connection const& conn, ResultPtr const& result);
      explicit Exception(Pipeline const& pipeline, ResultPtr const& result);
   };
} // namespace PC::asiopq