#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <memory>

#include <common/task_pool.hpp>
#include <tensor/permutation.hpp>
#include <tensor/fraction.hpp>
#include <tensor/symmetry.hpp>
#include <tensor/expression.hpp>

#include <vector/vector.hpp>
#include <vector/matrix.hpp>

namespace Construction {
	namespace Tensor {

		// Forward declarations
		class ScaledTensor;
		class AddedTensor;
		class MultipliedTensor;

		/**
			\class CannotAddTensorsException
		 */
		class CannotAddTensorsException : public Exception {
		public:
			CannotAddTensorsException() : Exception("Cannot add tensors due to incompatible indices") { }
		};

		/**
			\class CannotMultiplyTensorsException
		 */
		class CannotMultiplyTensorsException : public Exception {
		public:
			CannotMultiplyTensorsException() : Exception("Cannot multiply tensors due to incompatible indices") { }
		};

        /**
			\class CannotContractTensorsException
		 */
		class CannotContractTensorsException : public Exception {
		public:
			CannotContractTensorsException() : Exception("Cannot contract tensors due to incompatible indices") { }
		};

		// Evaluation function
		typedef std::function<double(const std::vector<unsigned>&)>	EvaluationFunction;

		/**
			\class AbstractTensor

		 	\brief Class for general tensors

		 	The general class for a tensor. A tensor consists of a name, a printable LaTeX
		 	text and especially a set of indices. In general the index assignment,
		 	accessed by the functor operator, first checks if the index
		 	assignment is valid and then returns zero.

		 	To get a specific tensor with other components, inherit from tensor
		 	and implement the virtual `Evaluate` method.

		 	Two tensors can be added if the same indices appear. Furthermore
		 	we can multiply a tensor by an arbitrary real number and two tensors
		 	with different indices. Since we deal with only covariant tensors,
		 	we do not implement the Einstein sum convention here. This may
		 	follow in the future.
		 */
		class AbstractTensor : public Printable, Serializable<AbstractTensor> {
		public:
			enum class TensorType {
				ADDITION = 1,
				MULTIPLICATION = 2,
				SCALED = 3,
				ZERO = 4,

				SCALAR = 101,
				NUMERIC = 102,

				EPSILON = 201,
				GAMMA = 202,
				EPSILONGAMMA = 203,
				DELTA = 204,

				SUBSTITUTE = 301,

				CUSTOM = -1
			};
		public:
			AbstractTensor() : Printable("") { }

			/**
				Constructor of a Tensor

			 	\param name			The name of the tensor
			 	\param printable	The printed text of the tensor
			 	\param indices		The indices of the tensor
			 */
			AbstractTensor(const std::string& name, const std::string& printable, const Indices& indices)
				: name(name), Printable(printable), indices(indices) { }

			// Copy constructor
			AbstractTensor(const AbstractTensor& other)
				: name(other.name), Printable(other.printed_text), indices(other.indices), type(other.type) { }

			// Move constructor
			AbstractTensor(AbstractTensor&& other)
				: name(std::move(other.name)), Printable(std::move(other.printed_text)), indices(std::move(other.indices)), type(std::move(other.type)) { }

			// Virtual destructor
			virtual ~AbstractTensor() { }
		public:
			// Copy assignment
			AbstractTensor& operator=(const AbstractTensor& other) {
				name = other.name;
				printed_text = other.printed_text;
				indices = other.indices;
				type = other.type;
				return *this;
			}

			// Move assignment
			AbstractTensor& operator=(AbstractTensor&& other) {
				name = std::move(other.name);
				printed_text = std::move(other.printed_text);
				indices = std::move(other.indices);
				type = std::move(other.type);
				return *this;
			}
		public:
			virtual std::unique_ptr<AbstractTensor> Clone() const {
				return std::unique_ptr<AbstractTensor>(new AbstractTensor(*this));
			}
		public:
			/**
				Check if two tensors are equal

			 	TODO: this is of course not really correct so far
			 		  since two tensors are only equal if all the
			 		  components are equal. Since we do not
			 		  know anything about components so far
			 		  we will change this in the future
			 */
			virtual bool operator==(const AbstractTensor& other) const {
				return name == other.name && printed_text == other.printed_text && indices == other.indices;
			}

			/**
				Check if two tensors are unequal
			 */
			virtual bool operator!=(const AbstractTensor& other) const {
				return name != other.name || printed_text != other.printed_text || indices != other.indices;
			}
		public:
			/**
				Check if two tensors are completely equal, i.e.
			 	all the components match
			 */
			bool IsEqual(const AbstractTensor& other) const {
				// If the indices do not match, the tensors are clearly not equal
				if (indices != other.indices) return false;

				// Get all index combinations
				auto combinations = GetAllIndexCombinations();

				// Iterate over all index combinations
				for (auto& combination : combinations) {
					// if the components do not match => return false
					if (Evaluate(combination) != other(combination)) return false;
				}

				// Tensors are equal
				return true;
			}
		public:
			Indices GetIndices() const { return indices; }
			std::string GetName() const { return name; }

			void SetName(const std::string& name) { this->name = name; }
			virtual void SetIndices(const Indices& indices) { this->indices = indices; }
		public:
			void PermuteIndices(const Permutation& permutation) {
				indices = permutation(indices);
			}
		public:
			/**
				\brief LaTeX output of a tensor

			 	For a tensor, the output function is overwritten in order to
			 	also include the indices. Note that so far all indices
			 	are covariant.
			 */
			virtual std::string ToString() const override {
				std::stringstream ss;
				ss << printed_text << indices;
				return ss.str();
			}
		protected:
			/**
				\brief Check if the index assignment is fine

			 	This method will check the index assignment (in form of
			 	variadic templates) by checking the ranges for each index.
			 	If this fails an exception is thrown.
			 */
			template<typename T, typename... Args>
			std::vector<unsigned> CheckIndices(T t, Args... args) const {
				return indices(t, args...);
			}
		public:
			/**
				Virtual method for tensor evaluation. Standard result is zero.

			 	\param indices	Vector with the index assigment, i.e unsigned ints
			 	\returns		The tensor component at this index assignment
			 */
			virtual Scalar Evaluate(const std::vector<unsigned>& indices) const {
				return 0;
			}

			/**
				\brief Syntactic sugar for tensor evaluation.

				Syntactic sugar for tensor evaluation. Allows users to
			 	directly enter the index assignment in the usual syntax.
			 	It will automatically check the indices and then evaluate
			 	the tensor.

			 	For example `T(1,2,3)` will output the 123 component of the
			 	tensor.
			 */
			template<typename T, typename... Args>
			Scalar operator()(T t, Args... args) const {
				auto indices = CheckIndices(t, args...);
				return Evaluate(indices);
			}

			/**
				\brief Evaluate the tensor with an IndexAssignments object

				Evaluate the tensor with an IndexAssignments object.
			 	In this case the values to a specific index are
			 	given as a std::map which allows the assignment to
			 	permutations, added and multiplied tensors.
			 */
			inline Scalar operator()(const IndexAssignments& assignment) const {
				auto result = assignment(indices);
				return Evaluate(result);
			}

			/**
				\brief Alias for index assignment

			 	Alias for index assignment. It simply calls the Evaluate function
			 	of the tensor class.

			 	\param indices	Vector with the index assigment, i.e unsigned ints
			 	\returns		The tensor component at this index assignment
			 */
			inline Scalar operator()(const std::vector<unsigned>& indices) const {
				return Evaluate(indices);
			}
		public:
			/**
				\brief Brings the indices in normal order
			 */
			virtual std::unique_ptr<AbstractTensor> Canonicalize() const {
				return std::unique_ptr<AbstractTensor>(new AbstractTensor(*this));
			}

            /**
                \brief Method that allows to simplify expressions on contractions

                Method that allows to simplify expressions on contractions.
                For example, the contraction with a Kronecker delta thus just
                sets the index on the other one.

                If nothing can be done, it returns nullptr.
             */
            virtual std::unique_ptr<AbstractTensor> ContractionHeuristics(const AbstractTensor& other) const {
                return nullptr;
            }
        public:
            bool IsCustomTensor() const { return type == TensorType::CUSTOM; }

            bool IsAddedTensor() const { return type == TensorType::ADDITION; }
            bool IsMultipliedTensor() const { return type == TensorType::MULTIPLICATION; }
            bool IsScaledTensor() const { return type == TensorType::SCALED; }
            bool IsZeroTensor() const { return type == TensorType::ZERO; }

            bool IsScalar() const { return type == TensorType::SCALAR; }
            bool IsNumeric() const { return type == TensorType::NUMERIC; }
			bool IsSubstitute() const { return type == TensorType::SUBSTITUTE; }

            bool IsEpsilonTensor() const { return type == TensorType::EPSILON; }
            bool IsEpsilonGammaTensor() const { return type == TensorType::EPSILONGAMMA; }
            bool IsGammaTensor() const { return type == TensorType::GAMMA; }
            bool IsDeltaTensor() const { return type == TensorType::DELTA; }

			std::string TypeToString() const {
				switch (type) {
					case TensorType::ADDITION: return "Addition";
					case TensorType::MULTIPLICATION: return "Multiplication";
					case TensorType::SCALED: return "Scaled";
					case TensorType::ZERO: return "Zero";
					case TensorType::SCALAR: return "Scalar";
					case TensorType::SUBSTITUTE: return "Substitute";
					case TensorType::GAMMA: return "Gamma";
					case TensorType::EPSILON: return "Epsilon";
					case TensorType::EPSILONGAMMA: return "EpsilonGamma";
					case TensorType::DELTA: return "Delta";
					default: return "Custom";
				}
			}

			TensorType GetType() const { return type; }
		public:
			/**
				\brief Multiplication of two tensors

			 	Multiplication of two tensors. It encapsulates the two tensors
			 	by saving smart pointers to them and when evaluating uses
			 	them to generate the result.

			 	Note that an CannotMultiplyTensorsException is thrown if the tensors
			 	have one or more common indices.

			 	\throws CannotMultiplyTensorsException
			 */
			static std::unique_ptr<AbstractTensor> Multiply(const AbstractTensor& one, const AbstractTensor& second);

			/**
				\brief Multiplication of a tensor by a real number

			 	Multiplication of a tensor by a real number. It stores a
			 	pointer to the tensor and the number and when evaluating uses
			 	them to calculate the result.
			 */
			static std::unique_ptr<AbstractTensor> Multiply(const AbstractTensor& one, const Scalar& c);

			/**
				\brief Addition of two tensors

			 	Addition of two tensors. It encapsulates the two tensors
			 	by saving smart pointers to them and when evaluating uses
			 	them to generate the result.

			 	Note that an CannotAddTensorsException is thrown if the tensors
			 	have different indices although the order does not matter.

			 	\throws CannotAddTensorsException
			 */
			static std::unique_ptr<AbstractTensor> Add(const AbstractTensor& one, const AbstractTensor& second);
		public:
			/**
				\brief Checks if all the ranges are equal

			 	Checks if all the ranges of the indices are equal. If we for example
			 	multiply the Levi-Civita symbol on the spatial slice with the
			 	metric in all spacetime we have two indices that range from
			 	0 to 3 and three from 1 to 3. In this case the function
			 	returns false.
			 */
			bool AllRangesEqual() const {
				for (auto& index : indices) {
					if (index.GetRange() != indices[0].GetRange()) return false;
				}
				return true;
			}

			/**
				\brief Returns all the possible index combinations for the tensor.

			 	Returns all the possible index combinations for the tensor.
			 	It uses a recursive inline methods that fixes the indices one by one
			 	until all indices have a value. In this case the combination is added to the
			 	result.
			 */
			std::vector<std::vector<unsigned>> GetAllIndexCombinations() const {
                return indices.GetAllIndexCombinations();
			}

			virtual std::vector<std::vector<unsigned>> GetAllInterestingIndexCombinations() const {
				return GetAllIndexCombinations();
			}

			/**
				\brief Checks if the tensor is identical to zero

			 	Checks if the tensor is identical to zero. For
			 	this it evaluates at all the possible index combinations
				and immediately returns false if one combination does
			 	not yield zero.
			 */
			bool IsZero() const {
				// Get all index combinations
				auto combinations = GetAllIndexCombinations();

				// Iterate over all combinations
				for (auto& combination : combinations) {
					auto r = Evaluate(combination);
					if (r.HasVariables() || r.ToDouble() != 0) return false;
					//if (Evaluate(combination) != 0) return false;
				}

				return true;
			}

			/*bool IsIndexEqual(const AbstractTensor& other) const {
				if (other.type != type) return false;
				return symmetries.IsEqual(indices, other.indices);
			}*/

			/*bool IsEqual(const Tensor& tensor) const {
				return (*this - tensor).IsZero();
			}*/
		public:
			void Serialize(std::ostream& os) const override;
			static std::unique_ptr<AbstractTensor> Deserialize(std::istream& is);
		protected:
			std::string name;
			Indices indices;

			TensorType type;

			//EvaluationFunction evaluator;
		};

		// Syntactic sugar for pointers to tensors
		typedef std::unique_ptr<AbstractTensor> TensorPointer;
		typedef std::unique_ptr<const AbstractTensor> ConstTensorPointer;

		/**
			\class AddedTensor

		 	The sum of two (arbitrary) tensors. This should never be explicitely
			constructed without garantueeing that the indices of both tensors
		    match.
		 */
		class AddedTensor : public AbstractTensor {
		public:
			/**
				Constructor of an AddedTensor
			 */
			AddedTensor(TensorPointer A, TensorPointer B)
				: AbstractTensor("", "", A->GetIndices()) {

				type = TensorType::ADDITION;
				summands.push_back(std::move(A));
				summands.push_back(std::move(B));
			}

			AddedTensor(std::vector<TensorPointer>&& vec, const Indices& indices) {
				type = TensorType::ADDITION;
				summands = std::move(vec);
                this->indices = indices;

				//if (summands.size() > 0) indices = summands[0]->GetIndices();
			}
		public:
			void AddFromRight(TensorPointer A) {
				summands.push_back(std::move(A));
			}

			void AddFromLeft(TensorPointer A) {
				summands.insert(summands.begin(), std::move(A));
			}

			virtual ~AddedTensor() = default;
		public:
			virtual TensorPointer Clone() const override {
				std::vector<TensorPointer> newSummands;

				for (auto& tensor : summands) {
					newSummands.push_back(std::move(tensor->Clone()));
				}

				return std::move(TensorPointer(new AddedTensor(std::move(newSummands), indices)));
			}
		public:
			/**
				Return the LaTeX code of both tensors
			 */
			virtual std::string ToString() const override;
		public:
			const TensorPointer& At(unsigned id) const { return summands[id]; }
			size_t Size() const { return summands.size(); }
		public:
			static void DoSerialize(std::ostream& os, const AddedTensor& tensor) {
				// Write size of
				size_t size = tensor.summands.size();
				os.write(reinterpret_cast<const char*>(&size), sizeof(size));

				for (int i=0; i<size; i++) {
					tensor.summands[i]->Serialize(os);
				}
			}

			static TensorPointer DoDeserialize(std::istream& is, const Indices& indices) {
				std::vector<TensorPointer> summands;

				// Read size
				size_t size;
				is.read(reinterpret_cast<char*>(&size), sizeof(size));

				// Deserialize all the summands
				for (int i=0; i<size; i++) {
					auto tensor = AbstractTensor::Deserialize(is)->Clone();
					if (!tensor) return nullptr;

					summands.push_back(std::move(tensor));
				}

				return TensorPointer(new AddedTensor(std::move(summands), indices));
			}
		public:
			/**
				Set the indices to the new order
			 */
			virtual void SetIndices(const Indices& newIndices) override {
                auto oldIndices = indices;
				indices = newIndices;

                // Generate mapping
                std::map<Index, Index> mapping;
                for (int i=0; i<oldIndices.Size(); i++) {
                    mapping[oldIndices[i]] = indices[i];
                }

				// Need to permute indices in all the summands
				for (auto& tensor : summands) {
					tensor->SetIndices(tensor->GetIndices().Shuffle(mapping));
				}
			}
		public:
			/**
            	\brief Evaluate the components of the sum

                Evaluate the components of the sum. It first checks the
                index assignment and afterwards generates an IndexAssignments
                instance. that is passed to both tensors.

                The reason for the IndexAssignments object is that we need
                terms as
                	 T_{ab} + T_{ba}
                This gives us a tensor with {ab} indices, but the assignment
            	has to incorporate the arrangement of the tensors.

            	\throws IncompleteIndexAssignmentException
             */
			virtual Scalar Evaluate(const std::vector<unsigned>& args) const override {
				// Get the indices
				auto indices = GetIndices();

				// If number of args and indices differ return
				if (args.size() != indices.Size()) {
					throw IncompleteIndexAssignmentException();
				}

				// Create index assignments
				IndexAssignments assignment;
				for (int i=0; i<args.size(); i++) {
					assignment[indices[i].GetName()] = args[i];
				}

				Scalar result = 0;
				for (auto& tensor : summands) {
					result += (*tensor)(assignment);
				}
				return result;
			}

			/**
				Canonicalize a sum of two tensors
			 */
			TensorPointer Canonicalize() const override {
				std::vector<TensorPointer> newSummands;

				for (auto& tensor : summands) {
					newSummands.push_back(std::move(tensor->Canonicalize()));
				}

				return std::move(TensorPointer(new AddedTensor(std::move(newSummands), indices)));
			}
		private:
			std::vector<TensorPointer> summands;
		};

		/**
		 	\class MultipliedTensor

		 	\brief Class for two multiplied tensors.

		 	Class for two multiplied tensors. This should never be
		 	created without garantueeing that each index occurs maximally
		 	once or otherwise the tensor cannot properly evaluated.

		 	Mathematically this means that the multiplication of two tensors
		 		S_{abc} T_{de}
		 	gives us a tensor with indices {abcde}
		 */
		class MultipliedTensor : public AbstractTensor {
		public:
			MultipliedTensor(TensorPointer A, TensorPointer B)
				: AbstractTensor("", "", A->GetIndices().Contract(B->GetIndices())), A(std::move(A)), B(std::move(B))
			{
				type = TensorType::MULTIPLICATION;
			}

			virtual ~MultipliedTensor() = default;
		public:
			virtual TensorPointer Clone() const override {
				return std::move(TensorPointer(new MultipliedTensor(
					std::move(A->Clone()),
					std::move(B->Clone())
				)));
			}
		public:
			virtual void SetIndices(const Indices& newIndices) override {
                // Generate mapping
                std::map<Index, Index> mapping;
                for (int i=0; i<indices.Size(); ++i) {
                    mapping[indices[i]] = newIndices[i];
                }

                indices = newIndices;
                A->SetIndices(A->GetIndices().Shuffle(mapping));
                B->SetIndices(B->GetIndices().Shuffle(mapping));
			}
		public:
			virtual std::string ToString() const override {
				std::stringstream ss;
				ss << A->ToString() << B->ToString();
				return ss.str();
			}

			/**
            	\brief Evaluates the tensor component

            	Evaluates the tensor components. For this, we first
            	check the index assignment, then seperate the
            	assignments into two IndexAssignments objects for each
            	tensor and assign them to the given tensors.

            	\throws IncompleteIndexAssignmentException
         	 */
			virtual Scalar Evaluate(const std::vector<unsigned>& args) const override {
				// If number of args and indices differ return
				if (args.size() != indices.Size()) {
					throw IncompleteIndexAssignmentException();
				}

                // Find contracted indices
                Indices contracted;
                for (auto& index : A->GetIndices()) {
                    if (!indices.ContainsIndex(index)) {
                        contracted.Insert(index);
                    }
                }

                // Prepare result
                Scalar result = 0;

                // Get all the contracted index combinations
                auto contractedArgs = contracted.GetAllIndexCombinations();
                bool containsContractions = contractedArgs.size() > 0;

                // If the tensor does not contain contractions
                if (!containsContractions) contractedArgs.push_back({0});

                // Merge with the given args
                for (auto& args_ : contractedArgs) {
                    IndexAssignments assignment1;
                    IndexAssignments assignment2;

                    // Set the value of the contracted indices
                    if (containsContractions) {
                        for (unsigned i=0; i<contracted.Size(); ++i) {
                            assignment1[contracted[i].GetName()] = args_[i];
                            assignment2[contracted[i].GetName()] = args_[i];
                        }
                    }

                    // Set the values of the rest
                    {
                        unsigned i=0;
                        auto indicesA = A->GetIndices();
                        auto indicesB = B->GetIndices();

                        // Iterate over all the indices
                        for (auto& index : indices) {
                            if (indicesA.ContainsIndex(index)) {
                                assignment1[index.GetName()] = args[i];
                            }

                            if (indicesB.ContainsIndex(index)) {
                                assignment2[index.GetName()] = args[i];
                            }

                            i++;
                        }
                    }

                    // Add this to the result
                    result += (*A)(assignment1) * (*B)(assignment2);
                }

				return result;
			}
		public:
			const TensorPointer& GetFirst() const {
				return A;
			}

			const TensorPointer& GetSecond() const {
				return B;
			}
		public:
			static void DoSerialize(std::ostream& os, const MultipliedTensor& tensor) {
				tensor.A->Serialize(os);
				tensor.B->Serialize(os);
			}

			static TensorPointer DoDeserialize(std::istream& is, const Indices& indices) {
				auto A = AbstractTensor::Deserialize(is)->Clone();
				auto B = AbstractTensor::Deserialize(is)->Clone();
				return TensorPointer(new MultipliedTensor(std::move(A), std::move(B)));
			}
		private:
			TensorPointer A;
			TensorPointer B;
		};

		/**
		 	\class ScaledTensor

		 	\brief Class for a tensor multiplied by a real number.

		 	Class for a tensor multiplied by a real number. This should never
		 	be created without garantueeing that each index occurs maximally
		 	once or otherwise the tensor cannot properly evaluated.
		 */
		class ScaledTensor : public AbstractTensor {
		public:
			ScaledTensor(ConstTensorPointer&& A, const Scalar& c)
				: AbstractTensor("", "", A->GetIndices()), A(std::move(A)), c(c) {

				type = TensorType::SCALED;
			}

			virtual ~ScaledTensor() = default;
		public:
			virtual TensorPointer Clone() const override {
				return TensorPointer(new ScaledTensor(
					std::move(A->Clone()),
					c
				));
			}
		public:
			/**
				\brief Evaluates the tensor component

				Evaluates the tensor components. We can directly
				do so by feeding the arguments to the tensor and
				multiplying the evaluated result with the scale
				factor c

				\throws IncompleteIndexAssignmentException
			 */
			virtual Scalar Evaluate(const std::vector<unsigned>& args) const override {
				return A->Evaluate(args) * c;
			}

			TensorPointer Canonicalize() const override {
				auto newA = A->Canonicalize();
				if (newA->IsScaledTensor()) {
					ScaledTensor* scaled = static_cast<ScaledTensor*>(newA.get());
					scaled->SetScale(c * scaled->GetScale());
				} else {
					newA = std::move(TensorPointer(new ScaledTensor(std::move(newA), c)));
				}
				return std::move(newA);
			}

			virtual std::string ToString() const override {
				std::stringstream ss;

				if (c.IsNumeric() && c.ToDouble() == 1) {
					// do nothing
				} else if (c.IsNumeric() && c.ToDouble() == -1) {
					ss << "-";
				} else {
					if (c.IsAdded()) {
						ss << "(" << c << ")" << " * ";
					} else {
						ss << c << " * ";
					}
				}

				// if sum of tensors, place brackets
				if (A->IsAddedTensor())
				ss << "(" << A->ToString() << ")";
				else ss << A->ToString();

				return ss.str();
			}
		public:
			virtual void SetIndices(const Indices& newIndices) override {
				indices = newIndices;

				// Also set the indices of the scaled tensor
				auto B = A->Clone();
				B->SetIndices(newIndices);

				A = std::move(B);
			}
		public:
			const ConstTensorPointer& GetTensor() const {
				return A;
			}

			Scalar GetScale() const { return c; }
			void SetScale(const Scalar& c) { this->c = c; }
		public:
			static void DoSerialize(std::ostream& os, const ScaledTensor& tensor) {
				tensor.c.Serialize(os);
				tensor.A->Serialize(os);
			}

			static TensorPointer DoDeserialize(std::istream& is, const Indices& indices) {
				// Deserialize the scale
				auto p = Scalar::Deserialize(is);
				if (!p) return nullptr;
				Scalar c = *static_cast<Scalar*>(p.get());

				// Deserialize the tensor
				auto A = AbstractTensor::Deserialize(is)->Clone();

				return TensorPointer(new ScaledTensor(std::move(A), c));
			}
		private:
			ConstTensorPointer A;
			Scalar c;
		};

		class ZeroTensor : public AbstractTensor {
		public:
			ZeroTensor() : AbstractTensor("0", "0", Indices()) { type = TensorType::ZERO; }

			virtual ~ZeroTensor() = default;
		public:
			virtual Scalar Evaluate(const std::vector<unsigned>& args) const override {
				return 0;
			}

			virtual std::string ToString() const override {
				return "0";
			}
		public:
			static void DoSerialize(std::ostream& os, const ZeroTensor& tensor) {
				// do nothing
			}

			static TensorPointer DoDeserialize(std::istream& is, const Indices& indices) {
				return TensorPointer(new ZeroTensor());
			}
		};

		/*MultipliedTensor AbstractTensor::operator*(const AbstractTensor &other) const {
			// Check if all indices are distinct
			for (auto& index : indices) {
				if (other.indices.ContainsIndex(index)) {
					throw CannotMultiplyTensorsException();
				}
			}

			return MultipliedTensor(
					std::move(Clone()),
					std::move(other.Clone())
			);
		}

		*/

		/*AddedTensor AbstractTensor::operator+(const AbstractTensor& other) const {
			// Tensors can only be added if it contains the same indices (but not necessarily in the same slots)
			if (!indices.IsPermutationOf(other.indices)) throw CannotAddTensorsException();
			return AddedTensor(
				std::move(Clone()),
				std::move(other.Clone())
			);
		}*/

		std::string AddedTensor::ToString() const {
			if (summands.size() == 0) return "";
			if (summands.size() == 1) return summands[0]->ToString();

			std::stringstream ss;
			ss << summands[0]->ToString();

			for (int i=1; i<summands.size(); i++) {
				if (summands[i]->IsScaledTensor() && static_cast<const ScaledTensor*>(summands[i].get())->GetScale() == Scalar(-1)) {
					ss << " - " << static_cast<const ScaledTensor*>(summands[i].get())->GetTensor()->ToString();
				} else ss << " + " << summands[i]->ToString();
			}

			return ss.str();
		}

		/**
		 	\class SubstituteTensor

		 	Tensor that only changes the index structure. This is
		 	just a helper class.
		 */
		class SubstituteTensor : public AbstractTensor {
		public:
			SubstituteTensor(TensorPointer A, const Indices& indices) : AbstractTensor("", "", indices), A(std::move(A)) {
				type = TensorType::SUBSTITUTE;

				if (!indices.IsPermutationOf(this->A->GetIndices())) {
					throw Exception("The indices have to be a permutation of each other");
				}
			}

			virtual ~SubstituteTensor() = default;
		public:
			virtual TensorPointer Clone() const override {
				return TensorPointer(new SubstituteTensor(std::move(A->Clone()), indices));
			}
		public:
			bool IsAddedTensor() const {
				return A->IsAddedTensor();
			}
		public:
			virtual std::string ToString() const override {
				return A->ToString();
			}

			virtual Scalar Evaluate(const std::vector<unsigned>& args) const override {
				// If number of args and indices differ return
				if (args.size() != indices.Size()) {
					throw IncompleteIndexAssignmentException();
				}

				// Create index assignments
				IndexAssignments assignment;
				for (int i=0; i<args.size(); i++) {
					assignment[indices[i].GetName()] = args[i];
				}

				return (*A)(assignment);
			}
		public:
			const TensorPointer& GetTensor() const {
				return A;
			}
		public:
			/**
				Set the indices to the new order
			 */
			virtual void SetIndices(const Indices& newIndices) override {
				// Need to permute the indices in A and B
				auto permutationA = Permutation::From(indices, A->GetIndices());

				indices = newIndices;
				A->SetIndices(permutationA(newIndices));
			}

			/**
				Get the indices of the substituted tensor
			 */
			Indices GetPermutedIndices() const {
				return A->GetIndices();
			}
		public:
			static void DoSerialize(std::ostream& os, const SubstituteTensor& tensor) {
				tensor.A->Serialize(os);
			}

			static TensorPointer DoDeserialize(std::istream& is, const Indices& indices) {
				auto A = AbstractTensor::Deserialize(is)->Clone();
				return TensorPointer(new SubstituteTensor(std::move(A), indices));
			}
		private:
			TensorPointer A;
		};

		/**
			\class ScalarTensor
		 */
		class ScalarTensor : public AbstractTensor {
		public:
			ScalarTensor(Scalar value) : value(value) { }

			ScalarTensor(const std::string& name, const std::string& printed_text, Scalar value)
				: AbstractTensor(name, printed_text, Indices()), value(value) {

				type = TensorType::SCALAR;
			}

			virtual ~ScalarTensor() = default;
		public:
			virtual TensorPointer Clone() const override {
				return TensorPointer(new ScalarTensor(*this));
			}
		public:
			virtual std::string ToString() const override {
				return printed_text;
			}
        public:
			virtual Scalar Evaluate(const std::vector<unsigned>& args) const override {
				return value;
			}
		public:
			Scalar operator()() const {
				return value;
			}
		public:
			static void DoSerialize(std::ostream& os, const ScalarTensor& tensor) {
				//os.write(reinterpret_cast<const char*>(&tensor.value), sizeof(tensor.value));
				tensor.value.Serialize(os);
			}

			static TensorPointer DoDeserialize(std::istream& is, const Indices& indices) {
				auto ptr = Scalar::Deserialize(is);
				if (!ptr) return nullptr;

				Scalar value = *static_cast<Scalar*>(ptr.get());

				return TensorPointer(new ScalarTensor(value));
			}
		private:
			Scalar value;
		};

		inline ScalarTensor One() {
			return ScalarTensor("1", "1", 1);
		}

		std::unique_ptr<AbstractTensor> AbstractTensor::Add(const AbstractTensor& one, const AbstractTensor& other) {
			// Clone the tensors
			auto first = one.Clone();
			auto second = other.Clone();

			// If one is the zero tensor
			if (one.IsZeroTensor()) return std::move(second);
			if (other.IsZeroTensor()) return std::move(first);

			// If the first one is an added tensor and the second isn't, simply add the new one to the other
			// and return the original pointer to keep memory allocation low
			if (first->IsAddedTensor() && !second->IsAddedTensor()) {
				static_cast<AddedTensor*>(first.get())->AddFromRight(std::move(second));
				return std::move(first);
			}

			// If the second one is an added tensor and the second isn't, simply add the new one to the other
			// and return the original pointer to keep memory allocation low
			if (second->IsAddedTensor() && !second->IsAddedTensor()) {
				static_cast<AddedTensor*>(first.get())->AddFromLeft(std::move(first));
				return std::move(second);
			}

			// If both are added tensors, move all the content from the right one into the left
			if (first->IsAddedTensor() && second->IsAddedTensor()) {
				AddedTensor* _second = static_cast<AddedTensor*>(second.get());

				for (int i=0; i<_second->Size(); i++) {
					static_cast<AddedTensor*>(first.get())->AddFromRight(std::move(_second->At(i)->Clone()));
				}

				return std::move(first);
			}

			// Else, return a new added tensor
			return TensorPointer(new AddedTensor(std::move(first), std::move(second)));
		}

		std::unique_ptr<AbstractTensor> AbstractTensor::Multiply(const AbstractTensor& one, const AbstractTensor& second) {
            // Check if it contains contractions
            bool containsContractions = false;
            for (auto& index : one.indices) {
                if (second.indices.ContainsIndex(index)) {
                    containsContractions = true;
                    break;
                }
            }

            // Try to apply heuristics
            {
                auto heuristics = one.ContractionHeuristics(second);
                if (heuristics != nullptr) return std::move(heuristics);

                heuristics = second.ContractionHeuristics(one);
                if (heuristics != nullptr) return std::move(heuristics);
            }

			// If one of the tensors is zero, return zero
			if (one.IsZeroTensor() || second.IsZeroTensor()) {
				return TensorPointer(new ZeroTensor());
			}

			return TensorPointer(new MultipliedTensor(
					std::move(one.Clone()),
					std::move(second.Clone())
			));
		}

		std::unique_ptr<AbstractTensor> AbstractTensor::Multiply(const AbstractTensor& one, const Scalar& c) {
			auto clone = one.Clone();

			// If the number is one do nothing
			if (c.ToDouble() == 1) return std::move(clone);

			// If the number is zero, return the zero tensor to free memory and simplify the evaluation
			if (c.IsNumeric() && c.ToDouble() == 0) return TensorPointer(new ZeroTensor());

			// If the tensor is zero, return zero
			if (one.IsZeroTensor()) return std::move(clone);

			// Syntactic sugar for scaling a scaled tensor
			if (one.IsScaledTensor()) {
				ScaledTensor* tensor = static_cast<ScaledTensor*>(clone.get());
				return TensorPointer(new ScaledTensor(
					std::move(tensor->GetTensor()->Clone()),
					tensor->GetScale() * c
				));
			}

			// Syntactic sugar for scaling a substitute tensor
			if (one.IsSubstitute()) {
				return TensorPointer(new SubstituteTensor(
					std::move(Multiply(*static_cast<SubstituteTensor*>(one.Clone().get())->GetTensor(), c)),
					one.GetIndices()
				));
			}

			return TensorPointer(new ScaledTensor(std::move(clone), c));
		}

		/**
			\class DeltaTensor

			\brief Represents the Kronecker delta tensor

			Represents the Kronecker delta tensor. It requires one index up,
			one down
		 */
		class DeltaTensor : public AbstractTensor {
		public:
			/**
				\brief Constructor of a delta tensor

				Constructor of a delta tensor. It takes a index collection of
				two indices. The first one will be made contravariant, the
				second one stays down.

				\param indices		The two indices of the delta
			 */
			DeltaTensor(const Indices& indices) : AbstractTensor("", "", indices) {
				assert(indices.Size() == 2);

				this->indices[0].SetContravariant(true);
				this->indices[1].SetContravariant(false);

				type = TensorType::DELTA;
			}

			/**
				Virtual destructor to properly release memory
			 */
			virtual ~DeltaTensor() = default;
		public:
			/**
				Returns a clone of the delta tensor
			 */
			virtual TensorPointer Clone() const override {
				return TensorPointer(new DeltaTensor(*this));
			}

			/**
				\brief Canonicalize the Kronecker delta

				Canonicalize the Kronecker delta. Since one index
				is up, the other one down, there is nothing to do.
			 */
			virtual TensorPointer Canonicalize() const override {
				return std::move(Clone());
			}

            /**
                \brief Heuristics for contractions with the Kronecker delta
             */
            virtual TensorPointer ContractionHeuristics(const AbstractTensor& other) const override {
                try {
                    auto contracted = indices.Contract(other.GetIndices());

                    // Clone the other tensor
                    auto clone = other.Clone();

                    // Set the indices
                    clone->SetIndices(contracted);
                    return std::move(clone);
                } catch (...) {
                    return nullptr;
                }
            }
		public:
			/**
				Evaluate the delta, by checking if the values are equal
			 */
			virtual Scalar Evaluate(const std::vector<unsigned>& args) const override {
				assert(args.size() == 2);
				return args[0] == args[1];
			}

			/**
				Print the tensor
			 */
			virtual std::string ToString() const override {
				std::stringstream ss;
				ss << "\\delta" << GetIndices();
				return ss.str();
			}
		};

        // Alias
		typedef DeltaTensor	KroneckerTensor;

		/**
			\class EpsilonTensor

		 	\brief Represents the totally antisymmetric tensor density

			Represents the totally antisymmetric tensor density also known as
		 	the Levi-Civita symbol. It is defined by
		 							   _
		 							  | +1  {a1...an} is an even permutation of {0...n}
		 		\epsilon_{a1...an} =  | -1  {a1...an} is an odd permutation of {0...n}
		 		                      |_ 0  else
		 */
		class EpsilonTensor : public AbstractTensor {
		public:
			EpsilonTensor(const Indices& indices)
				: AbstractTensor("epsilon", "\\epsilon", indices) {

				assert(indices[0].GetRange().GetTo()+1 -indices[0].GetRange().GetFrom() == indices.Size());
				type = TensorType::EPSILON;
			}

			virtual ~EpsilonTensor() = default;
		public:
			virtual TensorPointer Clone() const override {
				return TensorPointer(new EpsilonTensor(*this));
			}
		public:
			/**
			    \brief Evaluate the Levi-Civita symbol

			 	Evaluate the Levi-Civita symbol. For this we employ
			 	the representation given by

				\epsilon_{a_1...a_n} = \Pi_{1\le p < q \le n} \frac{a_p-a_q}{p-q}

				This is easier to calculate since no permutation have to
				be generated.
			 */
			static double GetEpsilonComponents(const std::vector<unsigned>& args) {
				double result = 1.0;
				for (unsigned p=0; p < args.size(); p++) {
					for (unsigned q=p+1; q < args.size(); q++ ) {
						result *= static_cast<double>(static_cast<int>(args[q])-static_cast<int>(args[p]))/(q-p);
					}
				}
				return result != 0 ? result : 0;
			}


			virtual Scalar Evaluate(const std::vector<unsigned>& args) const override {
				return GetEpsilonComponents(args);
            }

			virtual TensorPointer Canonicalize() const override {
				int sign = 1;

				// Sort indices
				auto sortedIndices = indices.Ordered();

				// Find the sign
				sign = Permutation::From(indices, sortedIndices).Sign();

				// Construct and return result
				if (sign < 0) {
					return std::move(TensorPointer(new ScaledTensor(
						std::move(TensorPointer(new EpsilonTensor(sortedIndices))),
						-1
					)));
				} else {
					return std::move(TensorPointer(new EpsilonTensor(sortedIndices)));
				}
			}

			/**
				Returns the Levi-Civita symbol in 3+1 dim spacetime, where
			 	the zeroth index is in temporal direction.
			 */
			static EpsilonTensor SpaceTime(int offset=0) {
				return EpsilonTensor(Indices::GetGreekSeries(4, {0,3}, offset));
			}

			/**
				Returns the Levi-Civita symbol on a spatial slice, i.e.
			 	in 3 dimensions.
			 */
			static EpsilonTensor Space(int offset=0) {
				return EpsilonTensor(Indices::GetRomanSeries(3, {1,3}, offset));
			}
		};

		// Alias
		typedef EpsilonTensor	LeviCivitaTensor;

		/**
			\class GammaTensor

		 	\brief Class for a flat space metric with (p,q) signature

		 	Class for a flat space metric. It has a (p,q) signature
		 */
		class GammaTensor : public AbstractTensor {
		public:
			GammaTensor()
				: AbstractTensor("gamma", "\\gamma", Indices::GetRomanSeries(2, {1,3})), signature({0,3}) {

                type = TensorType::GAMMA;
				std::vector<unsigned> _indices = {0,1};
            }

			GammaTensor(const Indices& indices, int p, int q)
				: AbstractTensor("gamma", "\\gamma", indices), signature({p,q}) {
				assert(indices.Size() == 2);

                type = TensorType::GAMMA;
				std::vector<unsigned> _indices = {0,1};
			}

			GammaTensor(const Indices& indices)
				: AbstractTensor("gamma", "\\gamma", indices),  signature({0,3}) {
				assert(indices.Size() == 2);

                type = TensorType::GAMMA;
				std::vector<unsigned> _indices = {0,1};
			}

			virtual ~GammaTensor() = default;
		public:
			virtual TensorPointer Clone() const override {
				return TensorPointer(new GammaTensor(*this));
			}
		public:
			std::pair<int, int> GetSignature() const { return signature; }
			void SetSignature(int p, int q) { signature = {p,q}; }
		public:
			/**
				Create a Euclidean metric, i.e. with signature (0,4)
			 */
			static GammaTensor EuclideanMetric(int offset=0) {
				return GammaTensor(Indices::GetGreekSeries(2, {0,3}, offset), 0,4);
			}

			/**
				Create a Minkowskian metric, i.e. with signature (1,3)
			 */
			static GammaTensor MinkowskianMetric(int offset=0) {
				return GammaTensor(Indices::GetGreekSeries(2, {0,3}, offset), 1,3);
			}

			/**
				Create a spatial metric, i.e. with signature (0,3)
			 */
			static GammaTensor SpatialMetric(int offset=0) {
				return GammaTensor(Indices::GetRomanSeries(2, {1,3}, offset), 0,3);
			}
		public:
			/**
				Evaluate the tensor components. It returns 0 if evaluated
			 	off diagonal, -1 for all indices < p and 1 else.
			 */
			virtual Scalar Evaluate(const std::vector<unsigned>& vec) const override {
				if (vec.size() != 2) {
					throw IncompleteIndexAssignmentException();
				}

				if (vec[0] == vec[1]) {
					if (vec[0]-indices[0].GetRange().GetFrom() < signature.first) return -1;
					else return 1;
				}
				return 0;
			}

			virtual TensorPointer Canonicalize() const override {
				auto sortedIndices = indices.Ordered();
				return std::move(TensorPointer(new GammaTensor(sortedIndices, signature.first, signature.second)));
			}
		public:
			static void DoSerialize(std::ostream& os, const GammaTensor& tensor) {
				int p = tensor.signature.first;
				int q = tensor.signature.second;

				os.write(reinterpret_cast<const char*>(&p), sizeof(p));
				os.write(reinterpret_cast<const char*>(&q), sizeof(q));
			}

			static TensorPointer DoDeserialize(std::istream& is, const Indices& indices) {
				int p, q;
				is.read(reinterpret_cast<char*>(&p), sizeof(p));
				is.read(reinterpret_cast<char*>(&q), sizeof(q));

				return TensorPointer(new GammaTensor(indices, p, q));
			}
		private:
			std::pair<int, int> signature;
		};

		// Alias
		typedef GammaTensor		MetricTensor;

		/**
			\class EpsilonGammaTensor

		 	Class to represent a multiplied version of epsilon and gamma
		 	tensors. Although it is possible to also obtain this with the
		 	help of EpsilonTensor and GammaTensor by multiplication,
		 	this allows us to directly put some optimizations in.

		 	For example we can immediately return the result in case
		 	the epsilon or one of the gammas vanish. It remains to be checked
		 	if a check of the indices will simplify the evaluation.
		 */
		class EpsilonGammaTensor : public AbstractTensor {
		public:
			EpsilonGammaTensor(unsigned numEpsilon, unsigned numGamma, const Indices& indices) : /*Tensor("EpsilonGamma", "\\epsilon\\gamma", indices)*/ AbstractTensor("", "", indices), numEpsilon(numEpsilon), numGamma(numGamma) {
				assert(numEpsilon * 3 + numGamma*2 == indices.Size());

				type = TensorType::EPSILONGAMMA;
			}

			EpsilonGammaTensor(const EpsilonGammaTensor& other)
					: AbstractTensor(other), numEpsilon(other.numEpsilon), numGamma(other.numGamma) { }
			EpsilonGammaTensor(EpsilonGammaTensor&& other)
					: AbstractTensor(std::move(other)), numEpsilon(std::move(other.numEpsilon)), numGamma(std::move(other.numGamma)) { }

			virtual ~EpsilonGammaTensor() = default;
		public:
			EpsilonGammaTensor& operator=(const EpsilonGammaTensor& other) {
				*this = other;
				numEpsilon = other.numEpsilon;
				numGamma = other.numGamma;
				return *this;
			}

			EpsilonGammaTensor& operator=(EpsilonGammaTensor&& other) {
				*this = std::move(other);
				numEpsilon = std::move(other.numEpsilon);
				numGamma = std::move(other.numGamma);
				return *this;
			}
		public:
			virtual TensorPointer Clone() const override {
				return TensorPointer(new EpsilonGammaTensor(*this));
			}
		public:
			virtual std::string ToString() const override {
				std::stringstream ss;
				unsigned pos = 0;

				for (unsigned i=0; i<numEpsilon; i++) {
					ss << "\\epsilon" << indices.Partial({pos,pos+2});
					pos += 3;
				}

				for (unsigned i=0; i<numGamma; i++) {
					ss << "\\gamma" << indices.Partial({pos, pos+1});
					pos += 2;
				}

				return ss.str();
			}
		public:
			unsigned GetNumEpsilons() const { return numEpsilon; }
			unsigned GetNumGammas() const { return numGamma; }
		public:
			static std::vector<unsigned> Partial(const std::vector<unsigned>& args, Range range) {
				std::vector<unsigned> result;
				for (auto i : range) {
					result.push_back(args[i]);
				}
				return result;
			}

			virtual Scalar Evaluate(const std::vector<unsigned>& args) const override {
				Scalar result = 1;
				unsigned pos = 0;

				// Calculate the epsilon contribution
				for (unsigned i=0; i<numEpsilon; i++) {
					auto indices = this->indices.Partial({pos,pos+2});
					auto partialArgs = Partial(args, {pos,pos+2});

					result *= EpsilonTensor::GetEpsilonComponents(partialArgs);

					if (result == 0.0) return result;

					pos += 3;
				}

				// Calculate the gamma contribution
				for (unsigned i=0; i<numGamma; i++) {
					auto indices = this->indices.Partial({pos, pos+1});
					auto partialArgs = Partial(args, {pos, pos+1});
					result *= GammaTensor(indices, 0,3).Evaluate(partialArgs);// GammaTensor::Evaluate(partialArgs, GammaTensor(indices, 0,3));

					if (result == 0.0) return result;

					pos += 2;
				}

				return result;
			}

			virtual void SetIndices(const Indices& indices) {
				this->indices = indices;
			}

			virtual TensorPointer Canonicalize() const override {
				unsigned pos = 0;
				int sign = 1;

				Indices newIndices;

				// Canonicalize the epsilon contribution
				if (numEpsilon == 1) {
					auto epsilonIndices = indices.Partial({0, 2});

					// Sort and append the indices to the new list
					auto sortedIndices = epsilonIndices.Ordered();
					newIndices.Append(sortedIndices);

					// Find the sign
					sign = Permutation::From(epsilonIndices, sortedIndices).Sign();

					pos += 3;
				}

				// Vector for the sorted indices of the gammas
				std::vector<Indices> gammas;

				// Canonicalize the gamma contribution
				for (unsigned i=0; i<numGamma; i++) {
					auto gammaIndices = indices.Partial({pos, pos+1});

					// Sort and append the indices to the new list
					auto sortedIndices = gammaIndices.Ordered();
					gammas.push_back(sortedIndices);

					pos += 2;
				}

				// Sort the indices of the gammas to respect the
				// commutivity of gammas
				std::sort(gammas.begin(), gammas.end(), [](const Indices& a, const Indices& b) {
					return a[0] < b[0];
				});

				// Append the now sorted gammas to all indices
				for (auto& gammaIndices : gammas) {
					newIndices.Append(gammaIndices);
				}

				// Construct and return result
				if (sign < 0) {
					return std::move(TensorPointer(new ScaledTensor(std::move(TensorPointer(new EpsilonGammaTensor(numEpsilon, numGamma, newIndices))), -1)));
				} else {
					return std::move(TensorPointer(new EpsilonGammaTensor(numEpsilon, numGamma, newIndices)));
				}
			}
		public:
			static void DoSerialize(std::ostream& os, const EpsilonGammaTensor& tensor) {
				unsigned numEpsilon = tensor.numEpsilon;
				unsigned numGamma = tensor.numGamma;

				os.write(reinterpret_cast<const char*>(&tensor.numEpsilon), sizeof(tensor.numEpsilon));
				os.write(reinterpret_cast<const char*>(&tensor.numGamma), sizeof(tensor.numGamma));
			}

			static TensorPointer DoDeserialize(std::istream& is, const Indices& indices) {
				unsigned numEpsilon, numGamma;
				is.read(reinterpret_cast<char*>(&numEpsilon), sizeof(numEpsilon));
				is.read(reinterpret_cast<char*>(&numGamma), sizeof(numGamma));

				return TensorPointer(new EpsilonGammaTensor(numEpsilon, numGamma, indices));

				/*unsigned numEpsilon = (indices.Size() % 2 == 0) ? 0 : 1;
				unsigned numGamma = (indices.Size() % 2 == 0) ? indices.Size()/2 : (indices.Size()-3)/2;
				return std::move(TensorPointer(new EpsilonGammaTensor(numEpsilon, numGamma, indices)));*/
			}
		private:
			unsigned numEpsilon;
			unsigned numGamma;
		};







		void AbstractTensor::Serialize(std::ostream& os) const {
			// Serialize name and printed_text
			os << name << ";" << printed_text << ";";

			// Serialize the indices
			indices.Serialize(os);

			// Write type
			int typeC = static_cast<int>(type);
			os.write(reinterpret_cast<const char*>(&typeC), sizeof(typeC));

			switch (type) {
				case AbstractTensor::TensorType::ADDITION:
					AddedTensor::DoSerialize(os, *static_cast<const AddedTensor*>(this));
					break;
				case AbstractTensor::TensorType::MULTIPLICATION:
					MultipliedTensor::DoSerialize(os, *static_cast<const MultipliedTensor*>(this));
					break;
				case AbstractTensor::TensorType::SCALED:
					ScaledTensor::DoSerialize(os, *static_cast<const ScaledTensor*>(this));
					break;
				case AbstractTensor::TensorType::SUBSTITUTE:
					SubstituteTensor::DoSerialize(os, *static_cast<const SubstituteTensor*>(this));
					break;
				case AbstractTensor::TensorType::ZERO:
					ZeroTensor::DoSerialize(os, *static_cast<const ZeroTensor*>(this));
					break;
				case AbstractTensor::TensorType::SCALAR:
					ScalarTensor::DoSerialize(os, *static_cast<const ScalarTensor*>(this));
					break;
				case AbstractTensor::TensorType::EPSILONGAMMA:
					EpsilonGammaTensor::DoSerialize(os, *static_cast<const EpsilonGammaTensor*>(this));
					break;
				case AbstractTensor::TensorType::GAMMA:
					GammaTensor::DoSerialize(os, *static_cast<const GammaTensor*>(this));
					break;

				default: break;
			}
		}

		std::unique_ptr<AbstractTensor> AbstractTensor::Deserialize(std::istream& is) {
			// Read name
			std::string name;
			std::getline(is, name, ';');

			// Read printed text
			std::string printed_text;
			std::getline(is, printed_text, ';');

			// Read indices
			auto indices = *Indices::Deserialize(is);

			// Read type
			int typeC;
			is.read(reinterpret_cast<char*>(&typeC), sizeof(typeC));
			TensorType type = static_cast<TensorType>(typeC);

			TensorPointer result;

			// If not a standard tensor, do the specific serialization stuff
			switch (type) {
				case TensorType::ADDITION:
					result = std::move(AddedTensor::DoDeserialize(is, indices));
					break;

				case TensorType::MULTIPLICATION:
					result = std::move(MultipliedTensor::DoDeserialize(is, indices));
					break;

				case TensorType::SCALED:
					result = std::move(ScaledTensor::DoDeserialize(is, indices));
					break;

				case TensorType::SCALAR:
					result = std::move(ScalarTensor::DoDeserialize(is, indices));
					break;

				case TensorType::GAMMA:
					result = std::move(GammaTensor::DoDeserialize(is, indices));
					break;

				case TensorType::EPSILONGAMMA:
					result = std::move(EpsilonGammaTensor::DoDeserialize(is, indices));
					break;

				case TensorType::SUBSTITUTE:
					result = std::move(SubstituteTensor::DoDeserialize(is, indices));
					break;

				case TensorType::ZERO:
					result = std::move(ZeroTensor::DoDeserialize(is, indices));
					break;

				default:
					auto t = TensorPointer(new AbstractTensor(name, printed_text, indices));
					t->SetName(name);
					t->SetPrintedText(printed_text);
					return std::move(t);
			}

			// Assign the name and printed text
			result->SetName(name);
			result->SetPrintedText(printed_text);

			return std::move(result);
		}

		class Tensor : public AbstractExpression {
		public:
			Tensor() : AbstractExpression(TENSOR), pointer(TensorPointer(new ZeroTensor())) { }
			Tensor(const std::string& name, const std::string& printable, const Indices& indices) : AbstractExpression(TENSOR), pointer(TensorPointer(new AbstractTensor(name, printable, indices))) { }

			Tensor(const Tensor& other) : AbstractExpression(TENSOR), pointer(std::move(other.pointer->Clone())) { }
			Tensor(Tensor&& other) : AbstractExpression(TENSOR), pointer(std::move(other.pointer)) { }

			virtual ~Tensor() = default;
		private:
			Tensor(TensorPointer pointer) : AbstractExpression(TENSOR), pointer(std::move(pointer)) { }
		public:
			Tensor& operator=(const Tensor& other) {
				pointer = std::move(other.pointer->Clone());
				return *this;
			}

			Tensor& operator=(Tensor&& other) {
				pointer = std::move(other.pointer);
				return *this;
			}
		public:
			virtual ExpressionPointer Clone() const override { return std::move(ExpressionPointer(new Tensor(pointer->Clone()))); }
		public:
			template<class T>
			T* As() {
				return static_cast<T*>(pointer.get());
			}

			template<class T>
			const T* As() const {
				return static_cast<const T*>(pointer.get());
			}
		private:
			typedef Scalar scalar_type;
		public:
			static Tensor Zero() { return Tensor(TensorPointer(new ZeroTensor())); }
			static Tensor One() { return Tensor(TensorPointer(new ScalarTensor(1))); }
			//static Tensor Scalar(const Scalar& c) { return Tensor(TensorPointer(new ScalarTensor(c))); }

			static Tensor Delta(const Indices& indices) { return Tensor(TensorPointer(new DeltaTensor(indices))); }
			static Tensor Epsilon(const Indices& indices) { return Tensor(TensorPointer(new EpsilonTensor(indices))); }
			static Tensor Gamma(const Indices& indices) { return Tensor(TensorPointer(new GammaTensor(indices))); }
			static Tensor Gamma(const Indices& indices, int p, int q) { return Tensor(TensorPointer(new GammaTensor(indices, p, q))); }
			static Tensor EpsilonGamma(unsigned numEpsilon, unsigned numGamma, const Indices& indices) {
				return Tensor(TensorPointer(new EpsilonGammaTensor(numEpsilon, numGamma, indices)));
			}

            static Tensor Contraction(const Tensor& tensor, const Indices& indices) {
                // Clone
                auto clone = tensor.pointer->Clone();

                // Set the tensor to the new indices
                clone->SetIndices(indices);

                // If there are no
                if (!indices.ContainsContractions()) {
                    return Tensor(std::move(clone));
                }

                // Syntactic sugar, just multiply by one, then the evaluation
                // magic makes everything correct
                return One() * Tensor(std::move(clone));
            }

			static Tensor Substitute(const Tensor& tensor, const Indices& indices) {
				// Syntactic sugar for addition
				if (tensor.IsAdded()) {
					Tensor result = Tensor::Zero();

					for (int i=0; i<tensor.As<AddedTensor>()->Size(); ++i) {
						result += Substitute(Tensor(tensor.As<AddedTensor>()->At(i)->Clone()), indices);
					}

					return result;
				}

				// Syntactic sugar for scaling
				if (tensor.IsScaled()) {
					return tensor.As<ScaledTensor>()->GetScale() * Substitute(Tensor(tensor.As<ScaledTensor>()->GetTensor()->Clone()), indices);
				}

				return Tensor(TensorPointer(new SubstituteTensor(std::move(tensor.pointer->Clone()), indices)));
			}
		public:
			bool IsCustom() const { return pointer->IsCustomTensor(); }

			bool IsAdded() const { return pointer->IsAddedTensor(); }
			bool IsMultiplied() const { return pointer->IsMultipliedTensor(); }
			bool IsScaled() const { return pointer->IsScaledTensor(); }
			bool IsZeroTensor() const { return pointer->IsZeroTensor(); }

			bool IsScalar() const { return pointer->IsScalar(); }
			bool IsNumeric() const { return pointer->IsNumeric(); }
			bool IsSubstitute() const { return pointer->IsSubstitute(); }

			bool IsEpsilon() const { return pointer->IsEpsilonTensor(); }
			bool IsGamma() const { return pointer->IsGammaTensor(); }
			bool IsEpsilonGamma() const { return pointer->IsEpsilonGammaTensor(); }
			bool IsDelta() const { return pointer->IsDeltaTensor(); }

			std::string TypeToString() const { return pointer->TypeToString(); }
		public:
			inline bool IsEqual(const Tensor& other) const { return pointer->IsEqual(*other.pointer); }

			inline Indices GetIndices() const { return pointer->GetIndices(); }
			inline std::string GetName() const { return pointer->GetName(); }
			inline void SetName(const std::string& name) { pointer->SetName(name); }
			inline void SetIndices(const Indices& indices) { pointer->SetIndices(indices); }

			inline void PermuteIndices(const Permutation& permutation) { pointer->PermuteIndices(permutation); }

			inline Tensor Canonicalize() const { return Tensor(std::move(pointer->Canonicalize())); }

			inline bool AllRangesEqual() const { return pointer->AllRangesEqual(); }

			inline std::vector<std::vector<unsigned>> GetAllIndexCombinations() const { return pointer->GetAllIndexCombinations(); }

			inline bool IsZero() const { return pointer->IsZero(); }
		public:
			virtual std::string ToString() const override {
                // Has variables?
                bool vars = HasVariables();

				// One summand per line
				auto summands = GetSummands();

				// Stop recursion if atomic tensor
				if (summands.size() == 1) {
					if (summands[0].IsZeroTensor()) return "0";
					return summands[0].pointer->ToString();
				}

				// Else, iterate over all summands
				std::stringstream ss;
				for (unsigned i=0; i<summands.size(); i++) {
					ss << summands[i];
					if (i < summands.size() - 1) ss << " + ";
                    if (vars) ss << std::endl;
				}
				return ss.str();
			}
		public:
			bool HasVariables() const {
                auto summands = GetSummands();

                for (auto& t : summands) {
                    if (t.IsScaled() && t.SeparateScalefactor().first.HasVariables()) return true;
                }

                return false;
			}

			/**
				\brief Splits the tensor in its summands

				\returns {std::vector<Tensor>}		List of all the tensors
			 */
			std::vector<Tensor> GetSummands() const {
				if (IsAdded()) {
					std::vector<Tensor> result;

					for (int i=0; i<As<AddedTensor>()->Size(); ++i) {
						result.push_back(Tensor(std::move(As<AddedTensor>()->At(i)->Clone())));
					}

					return result;
				} else {
					return { *this };
				}
			}

			/**
				\brief Expands the tensorial expression

				Expands the tensorial expression, but keeps brackets of scalars, i.e.

					Simplify((Gamma(a b) + Gamma(b a)) * Epsilon(c d e)) =
					Gamma(a b) * Epsilon(c d e) + Gamma(b a) * Epsilon(c d e)

				but
					(3 + a) * Gamma(b c)
				remains the same

				\returns {Tensor}	The expanded tensorial expression
			 */
			Tensor Expand() const {
				// Get the summands in the expression
				auto summands = GetSummands();

				// Start result
				Tensor result = Tensor::Zero();

				// Iterate over all summands
				for (auto& tensor : summands) {
					// If the tensor is scaled
					//
					if (tensor.IsScaled()) {
						ScaledTensor* _tensor = static_cast<ScaledTensor*>(tensor.pointer.get());
						scalar_type c = _tensor->GetScale();
						auto _summands = Tensor(std::move(_tensor->GetTensor()->Clone())).GetSummands();
						for (auto& _tensor : _summands) result += c * _tensor;
					} else if (tensor.IsMultiplied()) {
						MultipliedTensor* _tensor = static_cast<MultipliedTensor*>(tensor.pointer.get());

						if (!_tensor->GetFirst()->IsAddedTensor() && _tensor->GetSecond()->IsAddedTensor()) {
							auto first = Tensor(_tensor->GetFirst()->Clone());
							auto _summands = Tensor(std::move(_tensor->GetSecond()->Clone())).GetSummands();
							for (auto& second : _summands) result += first * second;
						} else if (_tensor->GetFirst()->IsAddedTensor() && !_tensor->GetSecond()->IsAddedTensor()) {
							auto second = Tensor(_tensor->GetSecond()->Clone());
							auto _summands = Tensor(std::move(_tensor->GetFirst()->Clone())).GetSummands();
							for (auto& first : _summands) result += first * second;
						} else if (_tensor->GetFirst()->IsAddedTensor() && _tensor->GetSecond()->IsAddedTensor()) {
							auto _summands1 = Tensor(std::move(_tensor->GetFirst()->Clone())).GetSummands();
							auto _summands2 = Tensor(std::move(_tensor->GetSecond()->Clone())).GetSummands();
							for (auto& first : _summands1) {
								for (auto& second : _summands2) {
									result += first * second;
								}
							}
						}
					} else {
						result += tensor;
					}
				}

				return result;
			}

			/**
				\brief Simplify the expression

				Simplifies the expression by factorizing it into a sum of the linear independent
				tensors. Note that this will get rid of expressions that are not obvious from the
				theory-wise point of view but are numerical facts. Since two tensors are, however,
				equal if they have the same components in one coordinate system, this is completely
				fine.

				\returns {Tensor}	The simplified tensorial expression
			 */
			Tensor Simplify() const {
				// Scaling heuristics
				if (IsScaled()) {
					auto it = SeparateScalefactor();
					return it.first * it.second.Simplify();
				};

				// Multiplied tensors heuristics
				if (IsMultiplied()) {
					return Tensor(static_cast<MultipliedTensor*>(pointer.get())->GetFirst()->Clone()).Simplify() * Tensor(static_cast<MultipliedTensor*>(pointer.get())->GetSecond()->Clone()).Simplify();
				}

				// If the tensor is not added, check if it is zero, otherwise no further simplification possible
				if (!IsAdded()) {
					return *this;
				}

				// Get the summands
				auto summands = GetSummands();

				// Initialize
                std::map<unsigned,Vector::Vector> vectors;

				// Get the indices of the resulting tensor
				auto indices = GetIndices();
				auto combinations = GetAllIndexCombinations();

				unsigned dimension = combinations.size();

				Vector::Matrix M (dimension, summands.size());

				// Insert the values into the matrix
				{
					Common::TaskPool pool;
					std::mutex mutex;

					for (int i=0; i<summands.size(); i++) {
						pool.Enqueue([&](unsigned id, const Tensor& tensor) {

							for (int j=0; j<dimension; j++) {
								IndexAssignments assignment;

								// Convert into index assignment
                        		int k = 0;
                        		for (auto &index : indices) {
                            		assignment[index.GetName()] = combinations[j][k];
                            		k++;
                        		}

                        		// Calculate the value of the assignment
                        		float value = tensor(assignment).ToDouble();

                        		// only lock and insert if necessary
                        		if (value != 0) {
                        			std::unique_lock<std::mutex> lock(mutex);

                        			// Insert the value into the matrix
                        			M(j,id) = value;
                        		}
							}

						}, i, summands[i].SeparateScalefactor().second);
					}

					pool.Wait();
				}

                // Reduce to reduced matrix echelon form
                M.ToRowEchelonForm();

                // Now start collecting the tensors
                Tensor result = Tensor::Zero();

                int k=0;

                std::vector<scalar_type> _map_scalars;
                std::vector<Tensor> _map_tensors;

                // Iterate over the rows
                unsigned max = std::min(static_cast<unsigned>(M.GetNumberOfRows()), static_cast<unsigned>(summands.size()));

                for (int currentRow=0; currentRow < max; currentRow++) {
                	// Initialize the next tensor
                	scalar_type scalar = 0;
                	Tensor tensor = Tensor::Zero();

                	bool foundBase=false;

                    for (int i=k; i<summands.size(); i++) {
                        if (M(currentRow,i) == 0) continue;
                        else if (M(currentRow,i) == 1 && !foundBase) {
                            // switch mode
                            foundBase = true;
                            k = i+1;

                            // Found a new base vector
                            scalar = summands[i].SeparateScalefactor().first;
                            tensor = summands[i].SeparateScalefactor().second;//.Simplify();
                        } else if (foundBase) {
                            if (std::fmod(M(currentRow,i),1) == 0) {
                                scalar += summands[i].SeparateScalefactor().first * Scalar(M(currentRow,i),1);
                            } else {
                                scalar += summands[i].SeparateScalefactor().first * M(currentRow,i);
                            }
                        } else if (i == summands.size()-1 && !foundBase) {
                            // If all the values were zero, no further information
                            // can be found in the matrix, thus break the loop
                            break;
                        } else {
                            // SOMETHING WENT WRONG!!!
                            // TODO: throw exception
                            return Tensor::Zero();
                        }
                    }

                	// Add to the tensor map
                	auto it = std::find(_map_scalars.begin(), _map_scalars.end(), scalar);
                	if (it == _map_scalars.end()) {
                		_map_scalars.push_back(scalar);
                		_map_tensors.push_back(tensor);
                	} else {
                		auto id = it - _map_scalars.begin();
                		_map_tensors[id] += tensor;
                	}
                }

                // Add everything to the result
                for (unsigned i=0; i<_map_scalars.size(); i++) {
                	result += _map_scalars[i] * _map_tensors[i];
                }

				return result;
			}

			inline std::pair<scalar_type, Tensor> SeparateScalefactor() const {
				if (pointer->IsScaledTensor()) {
					return { As<ScaledTensor>()->GetScale(), Tensor(TensorPointer(As<ScaledTensor>()->GetTensor()->Clone())) };
				} else if (pointer->IsSubstitute()) {
					auto res = Tensor(std::move(static_cast<SubstituteTensor*>(pointer.get())->GetTensor()->Clone())).SeparateScalefactor();
					return { res.first, Tensor::Substitute(res.second, GetIndices()) };
				} else {
					return { 1, *this };
				}
			}

            Tensor CollectByVariables() const {
                // Expand first
                auto expanded = Expand();

                auto summands = expanded.GetSummands();

                std::vector<Scalar> variables;
                std::vector<Tensor> tensors;
                Tensor rest = Tensor::Zero();

                // Iterate over all the summands
                for (auto& t : summands) {
                    auto s = t.SeparateScalefactor();
                    auto u = s.first.SeparateVariablesFromRest();

                    // Iterate over the variable/scalar pairs
                    for (auto& v : u.first) {
                        // Check if the variable has already ocurred once
                        auto it = std::find(variables.begin(), variables.end(), v.first);

                        // If no, add to the map, otherwise add the tensor
                        if (it == variables.end()) {
                            variables.push_back(v.first);
                            tensors.push_back(v.second * s.second);
                        } else {
                            tensors[it - variables.begin()] += v.second * s.second;
                        }
                    }

                    // Add the rest
                    rest += s.second * u.second;
                }

                // Combine
                Tensor result = Tensor::Zero();

                for (int i=0; i<variables.size(); ++i) {
                    result += variables[i] * tensors[i];
                }

                return result;
            }

			Tensor SubstituteVariable(const scalar_type& variable, const scalar_type& expression) const {
				auto summands = GetSummands();

				Tensor result = Tensor::Zero();

				for (auto& _tensor : summands) {
					auto tmp = _tensor.SeparateScalefactor();
					result += tmp.first.Substitute(variable, expression) * tmp.second;
				}

				return result;
			}

			Tensor SubstituteVariables(const std::vector<std::pair<scalar_type, scalar_type>>& substitutions) const {
				Tensor result = *this;

				for (auto& substitution : substitutions) {
					result = std::move(result.SubstituteVariable(substitution.first, substitution.second));
				}

				return result.CollectByVariables();
			}

			Tensor RedefineVariables(const std::string& name, int offset=0) const {
				// Separate the summands, if available
				auto summands = GetSummands();

				Tensor result = Tensor::Zero();
				unsigned variableCount = 1 + offset;

				for (auto& tensor : summands) {
					// If the tensor is scaled, redefine the free variable in front (if present)
					if (tensor.IsScaled() && tensor.As<ScaledTensor>()->GetScale().HasVariables()) {
						result += scalar_type(name, variableCount++) * Tensor(tensor.As<ScaledTensor>()->GetTensor()->Clone());
					} else if (tensor.IsMultiplied()) {
						MultipliedTensor* _tensor = static_cast<MultipliedTensor*>(tensor.pointer.get());
						auto first = Tensor(_tensor->GetFirst()->Clone()).SeparateScalefactor();
						auto second = Tensor(_tensor->GetSecond()->Clone()).SeparateScalefactor();
						if (first.first.HasVariables() || second.first.HasVariables()) {
							result += scalar_type(name, variableCount++) * first.second * second.second;
						} else result += first.second * second.second;
					} else {
						result += tensor;
					}
				}

				return result;
			}

			std::vector<std::pair<scalar_type, Tensor>> ExtractVariables(Tensor* inhomogeneousPart = nullptr) const {
				// First expand and get summands
				auto summands = GetSummands();

				// Start result
				std::vector<scalar_type> result_scalars;
				std::vector<Tensor> result_tensors;

				// Iterate over all the summands
				for (auto& _tensor : summands) {
					// Extract the prefactor
					auto tmp = _tensor.SeparateScalefactor();
					auto scalar = tmp.first;
					auto tensor = tmp.second;

					// Expand scalar part
					auto scalarSummands = scalar.GetSummands();
					for (auto& v : scalarSummands) {
						// If the scalar is a variable
						if (v.IsVariable()) {
							auto it = std::find(result_scalars.begin(), result_scalars.end(), v);
							if (it == result_scalars.end()) {
								result_scalars.push_back(v);
								result_tensors.push_back(tensor);
							} else {
								result_tensors[it - result_scalars.begin()] += tensor;
							}
						}
						// if the scalar is a number, just add the tensor to the inhomogeneous part
						else if (v.IsNumeric()) {
							if (inhomogeneousPart != nullptr) (*inhomogeneousPart) += _tensor;
						}
						// if the scalar is a multiplication, we knwo that at least one of both
						// factors is a variable
						else if (v.IsMultiplied()) {
							auto first = v.As<MultipliedScalar>()->GetFirst()->Clone();
							auto second = v.As<MultipliedScalar>()->GetSecond()->Clone();

							bool a1 = v.As<MultipliedScalar>()->GetFirst()->IsVariable();
							bool a2 = v.As<MultipliedScalar>()->GetFirst()->IsNumeric();

							bool b1 = v.As<MultipliedScalar>()->GetSecond()->IsVariable();
							bool b2 = v.As<MultipliedScalar>()->GetSecond()->IsNumeric();

							if (a1 && b2) {
								scalar_type s = scalar_type(std::move(first));
								Tensor newTensor = scalar_type(std::move(second)) * tensor;

								auto it = std::find(result_scalars.begin(), result_scalars.end(), s);
								if (it == result_scalars.end()) {
									result_scalars.push_back(s);
									result_tensors.push_back(newTensor);
								} else {
									result_tensors[it - result_scalars.begin()] += newTensor;
								}
							} else if (a2 && b1) {
								scalar_type s = scalar_type(std::move(second));
								Tensor newTensor = scalar_type(std::move(first)) * tensor;

								auto it = std::find(result_scalars.begin(), result_scalars.end(), s);
								if (it == result_scalars.end()) {
									result_scalars.push_back(s);
									result_tensors.push_back(newTensor);
								} else {
									result_tensors[it - result_scalars.begin()] += newTensor;
								}
							} else {
								// Throw exception, do not support quadratic terms
								assert(false);
							}
						}
					}
				}

				// Turn this into the result
				std::vector< std::pair<scalar_type, Tensor> > result;
				for (unsigned i=0; i<result_scalars.size(); i++) {
					result.push_back({ result_scalars[i], result_tensors[i] });
				}

				return result;
			}

			/**
				\brief Convert the tensorial equation into a homogeneous linear system


			 */
			std::pair< Vector::Matrix, std::vector<scalar_type> > ToHomogeneousLinearSystem() const {
				// First expand and get summands
				auto variables = ExtractVariables();

				// Get all the index assignments
				auto indices = GetIndices();
				auto combinations = GetAllIndexCombinations();

				// Get the dimensions of the system
				unsigned n = combinations.size();
				unsigned m = variables.size();

				// Create matrix
				Vector::Matrix M(n, m);
				std::vector<scalar_type> _variables;

				// Iterate over all variables
				int i = 0;
				for (auto& pair : variables) {
					_variables.push_back(pair.first);

					// Evaluate all the components
					for (int j=0; j<combinations.size(); j++) {
						IndexAssignments assignment;

                        // Convert into index assignment
                        int k = 0;
                        for (auto &index : indices) {
                            assignment[index.GetName()] = combinations[j][k];
                            k++;
                        }

                        // Plug the value of the assignment into the matrix
                        M(j,i) = (pair.second)(assignment).ToDouble();
					}

					i++;
				}

				return { M, _variables };
			}
		public:
			std::vector<Indices> PermuteIndices(const Indices& indices) const {
				auto tensorIndices = GetIndices();

				// Calculate the position of the indices to permute
				std::vector<unsigned> positionsToPermute;
				for (auto& index : indices) {
					positionsToPermute.push_back(tensorIndices.IndexOf(index) + 1);
				}

				std::vector<Indices> permutations;

				// Helper method
                std::function<void(unsigned,Indices,Indices)> fn = [&](unsigned i, Indices used, Indices unused) {
                    // if all indices are used,
                    if (unused.Size() == 0) {
                        permutations.push_back(used);
                    } else {
                        // if the current index is not part of the symmetrized indices, just insert and go to the next
                        if (std::find(positionsToPermute.begin(), positionsToPermute.end(), i+1) == positionsToPermute.end()) {
                            used.Insert(tensorIndices[i]);
                            unused.Remove(std::distance(unused.begin(), std::find(unused.begin(), unused.end(), tensorIndices[i])));
                            fn(i+1, used, unused);
                        } else {
                            // else, iterate over the indices to change
                            for (auto& k : positionsToPermute) {
                                Indices newUnused = unused;
                                Indices newUsed = used;

                                auto it = std::find(newUnused.begin(), newUnused.end(), tensorIndices[k-1]);

                                // if the index is unused, add it and call recursion for next index
                                if (it != newUnused.end()) {
                                    int pos = std::distance(newUnused.begin(), it);
                                    newUnused.Remove(std::distance(newUnused.begin(), it));
                                    newUsed.Insert(tensorIndices[k - 1]);
                                    fn(i + 1, newUsed, newUnused);
                                }
                            }
                        }
                    }
                };

                // Let the magic happen ...
                Indices empty = {};
                fn(0, {}, tensorIndices);

                return permutations;
			}

            /**
                \brief Symmetrizes the tensor in the given indices

                Symmetrizes the tensor in the given indices. It uses
                several heuristics in the different stages and heavily relies
                on the `Canonicalize` method to check if two tensors are equal.

                \param      indices         The indices to symmetrize over
                \returns    Tensor          The symmetrized tensor
             */
			Tensor Symmetrize(const Indices& indices) const {
				// Handle sums differently
				if (IsAdded()) {
					auto summands = GetSummands();

					std::vector<std::pair<scalar_type,Tensor>> symmetrizedSummands;
					bool hasSameScale=true;
					scalar_type overalScale = 0;

					// Symmetrize all the summands in parallel
					{
						Common::TaskPool pool (8);
						bool firstEntry = true;
						std::mutex mutex;

						symmetrizedSummands = pool.Map<std::pair<scalar_type,Tensor>, Tensor>(summands, [&](const Tensor& tensor) {
							auto result = tensor.Symmetrize(indices).SeparateScalefactor();

							// Extract the scale of the first entry
							{
								std::unique_lock<std::mutex> lock(mutex);
								if (firstEntry) {
									firstEntry = false;
									overalScale = result.first;
								}
							}

							// If this has a different scale, remember this
							if (overalScale != result.first) hasSameScale = false;

							return result;
						});
					}

					Tensor result = Tensor::Zero();

					// If all the tensors have the same scale we can collect them
					if (hasSameScale) {
						// Collect all summands on the stack
						std::vector<Tensor> stack;

						for (int i=0; i<symmetrizedSummands.size(); i++) {
							auto __summands = symmetrizedSummands[i].second.GetSummands();
							for (auto& s : __summands) {
								stack.push_back(std::move(s));
							}
						}

						std::vector< std::pair<scalar_type, Tensor> > reduced;
						scalar_type lastScale;
						bool allTheSameScale=true;

						{
							bool firstEntry = true;

							while (stack.size() > 0) {
								// Get the first element from the stack
								auto s = stack[0].SeparateScalefactor();
								Tensor current = std::move(s.second);
								Scalar scale = std::move(s.first);

								// Remove the first element
								stack.erase(stack.begin());

								// Iterate over all the other summands
								for (int i=0; i<stack.size(); i++) {
									auto t = stack[i].SeparateScalefactor();
									Tensor newTerm = std::move(t.second);
									Scalar newScale = std::move(t.first);

									// If the tensor is the same after canonicalization modulo scale,
									// change the scale of the original tensor and remove the new one
									// from the stack.
									if (newTerm.GetType() == current.GetType() && newTerm.GetIndices() == current.GetIndices()) {
										scale += newScale;
										stack.erase(stack.begin() + i);
										i--;
									}
								}

								if (!scale.IsNumeric() || scale.ToDouble() != 0) {
									if (firstEntry) {
										firstEntry = false;
										lastScale = scale;
									}

									if (lastScale != scale) {
										allTheSameScale = false;
									}
									reduced.push_back({ scale, current });
								}
							}
						}

						for (auto& pair : reduced) {
							if (allTheSameScale) result += pair.second;
							else result += pair.first * pair.second;
						}

						if (allTheSameScale) result *= lastScale;

						return overalScale * result;

					} else {
						// Iterate over all symmetrized pairs
						for (auto& pair : symmetrizedSummands) {
							result += pair.first * pair.second;
						}

						return result;
					}
				}

				// Handle scales
				if (IsScaled()) {
					auto s = SeparateScalefactor();
					auto t = s.second.Symmetrize(indices);

					// If the tensor is zero, do not care about the scale
					if (t.IsZeroTensor()) return t;

					return s.first * t;
				}

				// Do not waste time on zero tensor
				if (IsZeroTensor()) return *this;

				// Get the permutation of the tensor
				auto permutations = PermuteIndices(indices);

				// Prepare result
				Tensor result = Tensor::Zero();

				// Generate permuted summands
				{
					std::vector<Tensor> stack;

					// Move the tensors on the stack
					{
						Common::TaskPool pool (8);
						stack = pool.Map<Tensor,Indices>(permutations, [this](const Indices& indices) {
							Tensor clone = *this;
							clone.SetIndices(indices);
							return clone.Canonicalize();
						});
					}

					// Iterate over all
					while (stack.size() > 0) {
						// Get the first element from the stack
						auto s = stack[0].SeparateScalefactor();
						Tensor current = std::move(s.second);
						Scalar scale = std::move(s.first);

						// Remove the first element
						stack.erase(stack.begin());

						// Iterate over all the other summands
						for (int i=0; i<stack.size(); i++) {
							auto t = stack[i].SeparateScalefactor();
							Tensor newTerm = std::move(t.second);
							Scalar newScale = std::move(t.first);

							// If the tensor is the same after canonicalization modulo scale,
							// change the scale of the original tensor and remove the new one
							// from the stack.
							if (newTerm.GetType() == current.GetType() && newTerm.GetIndices() == current.GetIndices()) {
								scale += newScale;
								stack.erase(stack.begin() + i);
								i--;
							}
						}

						if (!scale.IsNumeric() || scale.ToDouble() != 0) {
							result += scale * current;
						}
					}
				}

				// Scale
				if (!result.IsZeroTensor()) {
					result = Scalar(1,permutations.size()) * result;
				}

				return result;
			}

            /**
                \brief Anti-symmetrizes the tensor in the given indices

                Anti-symmetrizes the tensor in the given indices. It uses
                several heuristics in the different stages and heavily relies
                on the `Canonicalize` method to check if two tensors are equal.

                \param      indices         The indices to anti-symmetrize over
                \returns    Tensor          The anti-symmetrized tensor
             */
			Tensor AntiSymmetrize(const Indices& indices) const {
                // Handle sums differently
				if (IsAdded()) {
					auto summands = GetSummands();

					std::vector<std::pair<scalar_type,Tensor>> symmetrizedSummands;
					bool hasSameScale=true;
					scalar_type overalScale = 0;

					// Symmetrize all the summands in parallel
					{
						Common::TaskPool pool (8);
						bool firstEntry = true;
						std::mutex mutex;

						symmetrizedSummands = pool.Map<std::pair<scalar_type,Tensor>, Tensor>(summands, [&](const Tensor& tensor) {
							auto result = tensor.AntiSymmetrize(indices).SeparateScalefactor();

							// Extract the scale of the first entry
							{
								std::unique_lock<std::mutex> lock(mutex);
								if (firstEntry) {
									firstEntry = false;
									overalScale = result.first;
								}
							}

							// If this has a different scale, remember this
                            if (overalScale != result.first && overalScale != -result.first) hasSameScale = false;

							return result;
						});
					}

					Tensor result = Tensor::Zero();

					// If all the tensors have the same scale we can collect them
					if (hasSameScale) {
						// Collect all summands on the stack
						std::vector<Tensor> stack;

						for (int i=0; i<symmetrizedSummands.size(); i++) {
							auto __summands = symmetrizedSummands[i].second.GetSummands();
							for (auto& s : __summands) {
                                if (symmetrizedSummands[i].first == overalScale)
								    stack.push_back(std::move(s));
                                else
                                    stack.push_back(-s);
							}
						}

						std::vector< std::pair<scalar_type, Tensor> > reduced;
						scalar_type lastScale;
						bool allTheSameScale=true;

						{
							bool firstEntry = true;

							while (stack.size() > 0) {
								// Get the first element from the stack
								auto s = stack[0].SeparateScalefactor();
								Tensor current = std::move(s.second);
								Scalar scale = std::move(s.first);

								// Remove the first element
								stack.erase(stack.begin());

								// Iterate over all the other summands
								for (int i=0; i<stack.size(); i++) {
									auto t = stack[i].SeparateScalefactor();
									Tensor newTerm = std::move(t.second);
									Scalar newScale = std::move(t.first);

									// If the tensor is the same after canonicalization modulo scale,
									// change the scale of the original tensor and remove the new one
									// from the stack.
									if (newTerm.GetType() == current.GetType() && newTerm.GetIndices() == current.GetIndices()) {
										scale += newScale;
										stack.erase(stack.begin() + i);
										i--;
									}
								}

								if (!scale.IsNumeric() || scale.ToDouble() != 0) {
									if (firstEntry) {
										firstEntry = false;
										lastScale = scale;
									}

									if (lastScale != scale && lastScale != -scale) {
										allTheSameScale = false;
									}
									reduced.push_back({ scale, current });
								}
							}
						}

						for (auto& pair : reduced) {
							if (allTheSameScale) {
                                if (pair.first == lastScale)
                                    result += pair.second;
                                else
                                    result += -pair.second;
                            }
							else result += pair.first * pair.second;
						}

						if (allTheSameScale) result *= lastScale;

						return overalScale * result;

					} else {
						// Iterate over all symmetrized pairs
						for (auto& pair : symmetrizedSummands) {
							result += pair.first * pair.second;
						}

						return result;
					}
				}

				// Handle scales
				if (IsScaled()) {
					auto s = SeparateScalefactor();
					auto t = s.second.AntiSymmetrize(indices);

					// If the tensor is zero, do not care about the scale
					if (t.IsZeroTensor()) return t;

					return s.first * t;
				}

				// Do not waste time on zero tensor
				if (IsZeroTensor()) return *this;

				// Get the permutation of the tensor
				auto permutations = PermuteIndices(indices);

				// Prepare result
				Tensor result = Tensor::Zero();

				// Generate permuted summands
				{
					std::vector<Tensor> stack;

					// Move the tensors on the stack
					{
						Common::TaskPool pool (8);
                        auto originalIndices = GetIndices();

						stack = pool.Map<Tensor,Indices>(permutations, [this, &originalIndices](const Indices& indices) {
							Tensor clone = *this;
							clone.SetIndices(indices);

                            int sign = Permutation::From(originalIndices, indices).Sign();

                            // Modify the sign if necessary
        					if (sign < 0) {
        						clone = -clone;
                            }

							return clone.Canonicalize();
						});
					}

					// Iterate over all
					while (stack.size() > 0) {
						// Get the first element from the stack
						auto s = stack[0].SeparateScalefactor();
						Tensor current = std::move(s.second);
						Scalar scale = std::move(s.first);

						// Remove the first element
						stack.erase(stack.begin());

						// Iterate over all the other summands
						for (int i=0; i<stack.size(); i++) {
							auto t = stack[i].SeparateScalefactor();
							Tensor newTerm = std::move(t.second);
							Scalar newScale = std::move(t.first);

							// If the tensor is the same after canonicalization modulo scale,
							// change the scale of the original tensor and remove the new one
							// from the stack.
							if (newTerm.GetType() == current.GetType() && newTerm.GetIndices() == current.GetIndices()) {
								scale += newScale;
								stack.erase(stack.begin() + i);
								i--;
							}
						}

						if (!scale.IsNumeric() || scale.ToDouble() != 0) {
							result += scale * current;
						}
					}
				}

				// Scale
				if (!result.IsZeroTensor()) {
					result = Scalar(1,permutations.size()) * result;
				}

				return result;
			}

            /**
                \brief Exchange symmetrize a tensor in the given indices

                Exchange symmetrizes the tensor
             */
            Tensor ExchangeSymmetrize(const Indices& from, const Indices& indices) const {
                if (IsAdded()) {
                    auto summands = GetSummands();

                    std::vector<std::pair<scalar_type,Tensor>> symmetrizedSummands;
					bool hasSameScale=true;
					scalar_type overalScale = 0;

					// Symmetrize all the summands in parallel
					{
						Common::TaskPool pool (1);
						bool firstEntry = true;
						std::mutex mutex;
                        auto originalIndices = GetIndices();

                        // Generate tensor mapping
                        std::map<Index, Index> mapping;
                        for (int i=0; i<indices.Size(); ++i) {
                            mapping[from[i]] = indices[i];
                        }

						symmetrizedSummands = pool.Map<std::pair<scalar_type,Tensor>, Tensor>(summands, [&](const Tensor& tensor) {
                            // Call exchange symmetrization on the summand
							auto result = tensor.ExchangeSymmetrize(tensor.GetIndices(), tensor.GetIndices().Shuffle(mapping)).SeparateScalefactor();

							// Extract the scale of the first entry
							{
								std::unique_lock<std::mutex> lock(mutex);

								if (firstEntry) {
									firstEntry = false;
									overalScale = result.first;
								}
							}

							// If this has a different scale, remember this
                            if (overalScale != result.first && overalScale != -result.first) hasSameScale = false;

							return result;
						});
					}

					Tensor result = Tensor::Zero();

                    if (hasSameScale) {
                        // Collect all summands on the stack
						std::vector<Tensor> stack;

						for (int i=0; i<symmetrizedSummands.size(); i++) {
							auto __summands = symmetrizedSummands[i].second.GetSummands();
							for (auto& s : __summands) {
                                if (symmetrizedSummands[i].first == overalScale)
								    stack.push_back(std::move(s));
                                else
                                    stack.push_back(-s);
							}
						}

						std::vector< std::pair<scalar_type, Tensor> > reduced;
						scalar_type lastScale;
						bool allTheSameScale=true;

						{
							bool firstEntry = true;

							while (stack.size() > 0) {
								// Get the first element from the stack
								auto s = stack[0].SeparateScalefactor();
								Tensor current = std::move(s.second);
								Scalar scale = std::move(s.first);

								// Remove the first element
								stack.erase(stack.begin());

								// Iterate over all the other summands
								for (int i=0; i<stack.size(); i++) {
									auto t = stack[i].SeparateScalefactor();
									Tensor newTerm = std::move(t.second);
									Scalar newScale = std::move(t.first);

									// If the tensor is the same after canonicalization modulo scale,
									// change the scale of the original tensor and remove the new one
									// from the stack.
									if (newTerm.GetType() == current.GetType() && newTerm.GetIndices() == current.GetIndices()) {
										scale += newScale;
										stack.erase(stack.begin() + i);
										i--;
									}
								}

								if (!scale.IsNumeric() || scale.ToDouble() != 0) {
									if (firstEntry) {
										firstEntry = false;
										lastScale = scale;
									}

									if (lastScale != scale && lastScale != -scale) {
										allTheSameScale = false;
									}
									reduced.push_back({ scale, current });
								}
							}
						}

						for (auto& pair : reduced) {
							if (allTheSameScale) {
                                if (pair.first == lastScale)
                                    result += pair.second;
                                else
                                    result += -pair.second;
                            }
							else result += pair.first * pair.second;
						}

						if (allTheSameScale) result *= lastScale;

						return overalScale * result;

                    } else {
                        // Iterate over all symmetrized pairs
						for (auto& pair : symmetrizedSummands) {
							result += pair.first * pair.second;
						}

						return result;
                    }
                }

                if (IsScaled()) {
                    auto s = SeparateScalefactor();
                    return s.first * s.second.ExchangeSymmetrize(from, indices);
                }

                // Do not waste time on zero tensor
				if (IsZeroTensor()) return *this;

                // Make the new tensor
                auto clone = *this;
                clone.SetIndices(indices);
                clone = clone.Canonicalize();

                // Check if the tensors are equal modulo scale
                if (clone.GetIndices() == GetIndices()) {
                    // Get the scales of the tensors
                    auto s_ = SeparateScalefactor();
                    auto scale1 = s_.first;
                    auto scale2 = clone.SeparateScalefactor().first;

                    // Add the two scales (will most certainly be 1 or 0)
                    auto newScale = Scalar(1,2) * (scale1 + scale2);

                    // Return the exchange symmetrized tensor
                    return newScale * s_.second;
                } else {
                    return Scalar(1,2) * ( *this + clone );
                }
            }
        public:
			void Serialize(std::ostream& os) const override {
				pointer->Serialize(os);
			}

			static std::unique_ptr<AbstractExpression> Deserialize(std::istream& is) {
				auto tensor = AbstractTensor::Deserialize(is);
				if (!tensor) return nullptr;
				return std::unique_ptr<AbstractExpression>(new Tensor(std::move(tensor)));
			}
		public:
			/** Comparison **/
			inline bool operator==(const Tensor& other) const {
				return (*pointer) == (*other.pointer);
			}

			inline bool operator!=(const Tensor& other) const {
				return (*pointer) != (*other.pointer);
			}

			/** Evaluation **/
			template<typename T, typename... Args>
			inline scalar_type operator()(T t, Args... args) const {
				return (*pointer)(t, args...);
			}

			inline scalar_type operator()(const IndexAssignments& assignment) const {
				return (*pointer)(assignment);
			}

			inline scalar_type operator()(const std::vector<unsigned>& indices) const {
				return (*pointer)(indices);
			}

            inline scalar_type operator()() const {
                if (pointer->GetIndices().Size() > 0) throw IncompleteIndexAssignmentException();
                return (*pointer)(std::vector<unsigned>());
            }

			/** Tensor Arithmetics **/
			Tensor& operator+=(const Tensor& other) {
				auto old = std::move(pointer);
				pointer = std::move(AbstractTensor::Add(*old, *other.pointer));
				return *this;
			}

			Tensor operator+(const Tensor& other) const {
				return Tensor(std::move(AbstractTensor::Add(*pointer, *other.pointer)));
			}

			Tensor& operator-=(const Tensor& other) {
				auto old = std::move(pointer);
				pointer = std::move(AbstractTensor::Add(*old, *AbstractTensor::Multiply(*other.pointer, -1)));
				return *this;
			}

			inline Tensor operator-(const Tensor& other) const {
				return (*this) + (-other);
			}

			inline Tensor operator-() const {
				return (-1) * (*this);
			}

			Tensor& operator*=(const scalar_type& c) {
				auto old = std::move(pointer);
				pointer = std::move(AbstractTensor::Multiply(*old, c));
				return *this;
			}

			Tensor operator*(const scalar_type& c) const {
				return Tensor(std::move(AbstractTensor::Multiply(*pointer, c)));
			}

			inline friend Tensor operator*(const scalar_type& c, const Tensor& other) {
				return other * c;
			}

			Tensor& operator*=(const Tensor& other) {
				auto old = std::move(pointer);
				pointer = std::move(AbstractTensor::Multiply(*old, *other.pointer));
				return *this;
			}

			Tensor operator*(const Tensor& other) const {
				return Tensor(std::move(AbstractTensor::Multiply(*pointer, *other.pointer)));
			}
		public:
			size_t Size() const {
				switch (pointer->GetType()) {
					case AbstractTensor::TensorType::ADDITION:
						return sizeof(*static_cast<AddedTensor*>(pointer.get()));
					case AbstractTensor::TensorType::MULTIPLICATION:
						return sizeof(*static_cast<MultipliedTensor*>(pointer.get()));
					case AbstractTensor::TensorType::SCALED:
						return sizeof(*static_cast<ScaledTensor*>(pointer.get()));
					case AbstractTensor::TensorType::ZERO:
						return sizeof(*static_cast<ZeroTensor*>(pointer.get()));

					case AbstractTensor::TensorType::SCALAR:
						return sizeof(*static_cast<ScalarTensor*>(pointer.get()));

					case AbstractTensor::TensorType::EPSILON:
						return sizeof(*static_cast<EpsilonTensor*>(pointer.get()));

					case AbstractTensor::TensorType::GAMMA:
						return sizeof(*static_cast<GammaTensor*>(pointer.get()));

					case AbstractTensor::TensorType::EPSILONGAMMA:
						return sizeof(*static_cast<EpsilonGammaTensor*>(pointer.get()));

					case AbstractTensor::TensorType::DELTA:
						return sizeof(*static_cast<DeltaTensor*>(pointer.get()));

					case AbstractTensor::TensorType::SUBSTITUTE:
						return sizeof(*static_cast<SubstituteTensor*>(pointer.get()));

					default:
						return 0;
				}
			}
		private:
			TensorPointer pointer;
		};

	}
}
