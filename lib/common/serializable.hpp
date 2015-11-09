#pragma once

#include <common/error.hpp>

namespace Construction {
    namespace Common {

        class WrongFormatException : public Exception {
        public:
            WrongFormatException() : Exception("Wrong format. Cannot serialize/deserialize") { }
        };

        template<typename T>
        class Serializable {
        public:
            /**
                Serialize the content into a ostream
             */
            virtual void Serialize(std::ostream& os) const = 0;

            /**
                Deserialize
             */
            static std::shared_ptr<T> Deserialize(std::istream& is) {
                return std::make_shared<T>();
            }
        public:
            template<typename S>
            void WriteBinary(std::ostream& os, S data) {
                os.write(reinterpret_cast<const char*>(&data), sizeof(data));
            }
        };

    }
}