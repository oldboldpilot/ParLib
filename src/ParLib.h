/*
 * ParLib.h
 *
 *  Created on: Nov 12, 2013
 *      Author: muyiwa
 */

#ifndef PARLIB_H_
#define PARLIB_H_

/*
 * ParLib.h
 *
 *  Created on: Nov 10, 2013
 *      Author: muyiwa
 */

#include<algorithm>
#include<thread>
#include<functional>
#include<iterator>
#include<future>
#include<utility>
#include<condition_variable>
#include<iostream>

namespace parallel {
	enum class ThreadTypes {
		standard, async
	};
	template<class InputIt, unsigned int blksz = 25, ThreadTypes tT = ThreadTypes::standard>
	class LaunchPolicies {
		public:
			ThreadTypes tTypes;
			unsigned long length;
			unsigned long hardware_threads = std::thread::hardware_concurrency();
			unsigned long min_per_thread;
			unsigned long max_threads;
			unsigned long num_threads;
			unsigned long block_size;
			void SetLaunchPolicies(InputIt beg, InputIt end) {
				tTypes = tT;
				length = std::distance(beg, end);
				hardware_threads = std::thread::hardware_concurrency();
				min_per_thread = blksz;
				max_threads = (length + min_per_thread) / min_per_thread;
				;
				num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
				block_size = length / num_threads;
			}
	};
	template<class InputIt>
	class LaunchPolicies<InputIt, 25, ThreadTypes::standard> {
		public:
			ThreadTypes tTypes;
			unsigned long length;
			unsigned long hardware_threads;
			unsigned long min_per_thread;
			unsigned long max_threads;
			unsigned long num_threads;
			unsigned long block_size;
			void SetLaunchPolicies(InputIt beg, InputIt end) {
				tTypes = ThreadTypes::standard;
				length = std::distance(beg, end);
				hardware_threads = std::thread::hardware_concurrency();
				min_per_thread = 25;
				max_threads = (length + min_per_thread) / min_per_thread;
				;
				num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
				block_size = length / num_threads;
			}
	};

	template<class InputIt, class UnaryFunction>
	struct foreach_block {
			void operator ()(InputIt beg, InputIt end, UnaryFunction f) {
				std::for_each(beg, end, f);
			}
	};

	template<class InputIt, class UnaryFunction, class Tpolicy = LaunchPolicies<InputIt> >
	UnaryFunction for_each(InputIt beg, InputIt end, UnaryFunction f) {
		Tpolicy Tp;
		Tp.SetLaunchPolicies(beg, end);
		if(!Tp.length)
			return f;

		std::vector < std::thread > threads(Tp.num_threads - 1);
		InputIt block_start = beg;
		InputIt block_end = beg;
		for(int i; i < (Tp.num_threads - 1); i++) {

			std::advance(block_end, Tp.block_size);
			threads[i] = std::thread(foreach_block<InputIt, UnaryFunction>(), block_start,
					block_end, f);

			block_start = block_end;
		}
		foreach_block<InputIt, UnaryFunction>()(block_start, end, f);
		std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));

		return f;
	}

	template<class InputIt, class OutputIt, class UnaryOperator>
	struct transform_block {
			void operator()(InputIt first1, InputIt last1, OutputIt result, UnaryOperator op) {
				std::transform(first1, last1, result, op);

			}
	};
	template<class InputIt, class InputIt2, class OutputIt, class BinaryOperator>
	struct transform_block2 {
			void operator()(InputIt first1, InputIt last1, InputIt2 first2, OutputIt result,
					BinaryOperator op) {
				std::transform(first1, last1, first2, result, op);

			}
	};

	template<class InputIt, class OutputIt, class UnaryOperator, class Tpolicy = LaunchPolicies<
			InputIt>>
	OutputIt transform(InputIt beg, InputIt end, OutputIt result, UnaryOperator op) {
		Tpolicy Tp;
		Tp.SetLaunchPolicies(beg, end);
		if(!Tp.length)
			return result;

		std::vector < std::thread > threads(Tp.num_threads - 1);
		InputIt block_start = beg;
		InputIt block_end = beg;
		InputIt last = end;

		OutputIt outblock_start = result;
		for(int i = 0; i < (Tp.num_threads - 1); i++) {

			std::advance(block_end, Tp.block_size);
			threads[i] = std::thread(transform_block<InputIt, OutputIt, UnaryOperator>(),
					block_start, block_end, outblock_start, op);
			block_start = block_end;
			std::advance(outblock_start, Tp.block_size);
		}
		transform_block<InputIt, OutputIt, UnaryOperator>()(block_start, last, outblock_start,
				op);
		std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));

		return result;
	}

	template<class InputIt, class InputIt2, class OutputIt, class BinaryOperator,
			class Tpolicy = LaunchPolicies<InputIt>>
	OutputIt transform(InputIt beg1, InputIt end1, InputIt2 beg2, OutputIt result,
			BinaryOperator op) {
		Tpolicy Tp;
		Tp.SetLaunchPolicies(beg1, end1);
		if(!Tp.length)
			return result;

		std::vector < std::thread > threads(Tp.num_threads - 1);
		InputIt block_start1 = beg1;
		InputIt2 block_start2 = beg2;
		InputIt block_end1 = beg1;
		InputIt last1 =end1;


		OutputIt outblock_start = result;
		for(int i = 0; i < (Tp.num_threads - 1); i++) {

			std::advance(block_end1, Tp.block_size);
			threads[i] = std::thread(
					transform_block2<InputIt, InputIt2, OutputIt, BinaryOperator>(), block_start1,
					block_end1, block_start2, outblock_start, op);

			block_start1 = block_end1;
			std::advance(block_start2, Tp.block_size);
			std::advance(outblock_start, Tp.block_size);
		}
		transform_block2<InputIt, InputIt2, OutputIt, BinaryOperator>()(block_start2, last1,
				block_start2, outblock_start, op);
		std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));

		return result;
	}

	template<class InputIt, class T>
		struct accumulate_block {
				T operator()(InputIt beg, InputIt end, T &ret) {

					ret =std::accumulate(beg, end, ret);
					return ret;

				}
		};

	template<class InputIt, class T, class BinaryOperator>
			struct accumulate_block2 {
					T operator()(InputIt beg, InputIt end, T &ret, BinaryOperator op) {

						ret= std::accumulate(beg, end, ret,op);
						return ret;

					}
			};



	template<class InputIt, class T, class Tpolicy = LaunchPolicies<
				InputIt>>
		T accumulate(InputIt beg, InputIt end, T init) {
			Tpolicy Tp;
			Tp.SetLaunchPolicies(beg, end);
			if(!Tp.length)
				return init;

			std::vector < std::thread > threads(Tp.num_threads - 1);
			std::vector <T> output(Tp.num_threads);
			InputIt block_start = beg;
			InputIt block_end = beg;
			InputIt last = end;


			for(int i = 0; i < (Tp.num_threads - 1); i++) {

				std::advance(block_end, Tp.block_size);
				threads[i] = std::thread(accumulate_block<InputIt, T>(),
						block_start, block_end, std::ref(output[i]));
				block_start = block_end;
			}
			accumulate_block<InputIt, T>()(block_start, last, std::ref(output[Tp.num_threads-1]));
			std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));


			return std::accumulate(output.begin(),output.end(),init);
		}

	template<class InputIt, class T, class BinaryOperator, class Tpolicy = LaunchPolicies<
					InputIt>>
			T accumulate(InputIt beg, InputIt end, T init, BinaryOperator op) {
				Tpolicy Tp;
				Tp.SetLaunchPolicies(beg, end);
				if(!Tp.length)
					return init;

				std::vector < std::thread > threads(Tp.num_threads - 1);
				std::vector <T> output(Tp.num_threads);
				InputIt block_start = beg;
				InputIt block_end = beg;
				InputIt last = end;


				for(int i = 0; i < (Tp.num_threads - 1); i++) {

					std::advance(block_end, Tp.block_size);
					threads[i] = std::thread(accumulate_block2<InputIt, T, BinaryOperator>(),
							block_start, block_end, std::ref(output[i]),op);
					block_start = block_end;
				}
				accumulate_block2<InputIt, T, BinaryOperator>()(block_start, last, std::ref(output[Tp.num_threads-1]),op);
				std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));


				return std::accumulate(output.begin(),output.end(),init,op);
			}


}


#endif /* PARLIB_H_ */