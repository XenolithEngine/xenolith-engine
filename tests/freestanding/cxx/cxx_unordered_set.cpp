/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#include <sprt/cxx/cstring>
#include <sprt/runtime/stream.h>
#include <sprt/cxx/unordered_set>

namespace sprt {

static void edgecases1() {
	using unordered_set = __malloc_unordered_set<int>;

	sprt::cout << "=== edgecases1 ===\n";

	unordered_set set1;
	set1.rehash(10);
	set1.max_load_factor(10.0f);

	set1.insert({2, 12, 22, 32, 3, 5, 8});

	for (auto &it : set1) { sprt::cout << it << " "; }
	sprt::cout << "\n";

	set1.erase(12);

	for (auto &it : set1) { sprt::cout << it << " "; }
	sprt::cout << "   " << (set1.find(3) != set1.end()) << "\n";
	sprt::cout << "=== edgecases1 complete ===\n";
}

/* Some real data failure pattern */
static void edgecases2() {
	using unordered_set = __malloc_unordered_set<uintptr_t>;

	unordered_set set;
	set.max_load_factor(2.0f);
	set.emplace(2'138'142'832'798);
	set.emplace(2'138'142'832'802);
	set.emplace(2'138'142'832'806);
	set.emplace(2'138'142'832'809);
	set.emplace(2'138'142'832'813);
	set.emplace(2'138'142'832'817);
	set.emplace(2'138'142'832'821);
	set.emplace(2'138'142'832'824);
	set.emplace(2'138'142'832'828);
	set.emplace(2'138'142'832'832);
	set.emplace(2'138'142'832'836);
	set.emplace(2'140'290'327'039);
	set.emplace(2'141'364'060'669);
	set.emplace(2'141'364'060'681);
	set.emplace(2'141'364'060'693);
	set.emplace(2'138'142'832'858);
	set.emplace(2'140'290'327'049);
	set.emplace(2'141'364'064'509);
	set.emplace(2'140'290'324'499);
	set.emplace(2'141'364'062'979);
	set.emplace(2'140'290'325'134);
	set.emplace(2'141'364'062'211);
	set.emplace(2'140'290'324'504);
	set.emplace(2'140'290'325'139);
	set.emplace(2'141'364'062'991);
	set.emplace(2'140'290'324'509);
	set.emplace(2'140'290'324'514);
	set.emplace(2'141'364'062'223);
	set.emplace(2'140'290'325'144);
	set.emplace(2'140'290'325'149);
	set.emplace(2'140'290'327'054);
	set.emplace(2'140'290'327'689);
	set.emplace(2'141'364'063'747);
	set.emplace(2'140'290'327'694);
	set.emplace(2'141'364'063'759);
	set.emplace(2'140'290'327'699);
	set.emplace(2'140'290'327'704);
	set.emplace(2'137'069'092'623);
	set.emplace(2'137'069'094'408);
	set.emplace(2'137'069'092'175);
	set.emplace(2'138'142'835'521);
	set.emplace(2'133'847'867'468);
	set.emplace(2'133'847'868'524);
	set.emplace(2'133'847'867'121);
	set.emplace(2'133'847'869'439);
	set.erase(2'140'290'327'049);
	set.erase(2'140'290'327'039);
	set.emplace(2'140'290'325'769);
	set.emplace(2'133'847'870'143);
	set.emplace(2'140'290'325'164);
	set.emplace(2'135'995'354'355);
	set.emplace(2'140'290'325'249);
	set.emplace(2'140'290'325'334);
	set.emplace(2'133'847'867'831);
	set.emplace(2'133'847'869'791);
	set.emplace(2'140'290'327'079);
	set.emplace(2'141'364'064'527);
	set.emplace(2'140'290'327'084);
	set.emplace(2'141'364'064'539);
	set.emplace(2'140'290'327'089);
	set.emplace(2'141'364'064'551);
	set.emplace(2'140'290'327'094);
	set.emplace(2'133'847'868'865);
	set.emplace(2'140'290'327'109);
	set.emplace(2'140'290'327'114);
	set.emplace(2'140'290'327'119);
	set.emplace(2'140'290'327'124);
	set.emplace(2'140'290'327'134);
	set.emplace(2'140'290'324'519);
	set.emplace(2'135'995'352'464);
	set.emplace(2'135'995'352'467);
	set.emplace(2'142'437'802'660);
	set.emplace(2'140'290'327'144);
	set.emplace(2'140'290'327'154);
	set.emplace(2'140'290'327'164);
	set.emplace(2'140'290'327'174);
	set.emplace(2'140'290'327'184);
	set.emplace(2'140'290'327'194);
	set.emplace(2'140'290'327'204);
	set.emplace(2'130'626'643'200);
	set.emplace(2'130'626'643'456);
	set.emplace(2'130'626'643'712);
	set.emplace(2'130'626'643'462);
	set.emplace(2'130'626'643'208);
	set.emplace(2'130'626'643'468);
	set.emplace(2'133'847'870'152);
	set.emplace(2'130'626'642'190);
	set.emplace(2'130'626'641'938);
	set.emplace(2'130'626'642'710);
	set.emplace(2'130'626'643'718);
	set.emplace(2'130'626'641'706);
	set.emplace(2'130'626'642'956);
	set.erase(2'140'290'327'194);
	set.erase(2'140'290'327'174);
	set.erase(2'140'290'327'154);
	set.erase(2'140'290'327'134);
	set.emplace(2'141'364'071'571);
	set.emplace(2'140'290'334'989);
	set.emplace(2'141'364'071'583);
	set.emplace(2'140'290'334'994);
	set.emplace(2'141'364'071'595);
	set.emplace(2'140'290'334'999);
	set.emplace(2'144'585'296'236);
	set.emplace(2'134'921'617'315);
	set.emplace(2'134'921'617'321);
	set.emplace(2'140'290'330'519);
	set.emplace(2'140'290'330'524);
	set.emplace(2'140'290'325'434);
	set.emplace(2'140'290'330'534);
	set.emplace(2'140'290'330'544);
	set.emplace(2'140'290'330'554);
	set.emplace(2'140'290'330'564);
	set.emplace(2'140'290'330'574);
	set.emplace(2'140'290'330'584);
	set.emplace(2'140'290'330'594);
	set.emplace(2'140'290'330'604);
	set.emplace(2'140'290'330'614);
	set.emplace(2'140'290'330'624);
	set.emplace(2'140'290'330'634);
	set.emplace(2'140'290'330'644);
	set.emplace(2'140'290'330'654);
	set.emplace(2'140'290'330'664);
	set.emplace(2'140'290'330'674);
	set.emplace(2'140'290'330'684);
	set.emplace(2'140'290'330'694);
	set.emplace(2'140'290'330'704);
	set.emplace(2'140'290'330'714);
	set.emplace(2'140'290'330'724);
	set.emplace(2'140'290'330'734);
	set.emplace(2'140'290'330'744);
	set.emplace(2'140'290'330'754);
	set.emplace(2'140'290'330'764);
	set.emplace(2'140'290'330'774);
	set.emplace(2'140'290'330'784);
	set.emplace(2'140'290'330'794);
	set.emplace(2'140'290'330'804);
	set.emplace(2'140'290'330'814);
	set.emplace(2'140'290'330'824);
	set.emplace(2'140'290'330'834);
	set.emplace(2'140'290'330'844);
	set.emplace(2'140'290'330'854);
	set.emplace(2'140'290'330'864);
	set.emplace(2'140'290'330'874);
	set.emplace(2'140'290'330'884);
	set.emplace(2'140'290'330'894);
	set.emplace(2'140'290'330'904);
	set.emplace(2'140'290'330'914);
	set.emplace(2'140'290'330'924);
	set.emplace(2'140'290'330'934);
	set.emplace(2'140'290'330'944);
	set.emplace(2'140'290'327'219);
	set.emplace(2'140'290'325'449);
	set.erase(2'140'290'330'934);
	set.erase(2'140'290'330'914);
	set.erase(2'140'290'330'894);
	set.erase(2'140'290'330'874);
	set.erase(2'140'290'330'854);
	set.erase(2'140'290'330'834);
	set.erase(2'140'290'330'814);
	set.erase(2'140'290'330'794);
	set.erase(2'140'290'330'774);
	set.erase(2'140'290'330'754);
	set.erase(2'140'290'330'734);
	set.erase(2'140'290'330'714);
	set.erase(2'140'290'330'694);
	set.erase(2'140'290'330'674);
	set.erase(2'140'290'330'654);
	set.erase(2'140'290'330'634);
	set.erase(2'140'290'330'614);
	set.erase(2'140'290'330'594);
	set.erase(2'140'290'330'574);
	set.erase(2'140'290'330'554);
	set.erase(2'140'290'330'534);
	set.erase(2'140'290'324'519);
	set.emplace(2'141'364'069'393);
	set.emplace(2'140'290'325'864);
	set.emplace(2'140'290'325'869);
	set.emplace(2'140'290'325'874);
	set.emplace(2'140'290'335'134);
	set.erase(2'140'290'325'874);
	set.emplace(2'140'290'324'544);
	set.emplace(2'140'290'327'224);
	set.erase(2'140'290'325'434);
	set.emplace(2'140'290'324'554);
	set.emplace(2'140'290'324'564);
	set.emplace(2'140'290'324'574);
	set.emplace(2'140'290'324'584);
	set.emplace(2'140'290'324'594);
	set.emplace(2'140'290'324'604);
	set.emplace(2'140'290'324'614);
	set.emplace(2'140'290'324'624);
	set.emplace(2'140'290'324'634);
	set.emplace(2'140'290'324'644);
	set.emplace(2'140'290'324'654);
	set.emplace(2'140'290'324'664);
	set.emplace(2'140'290'324'674);
	set.emplace(2'140'290'324'684);
	set.emplace(2'140'290'324'694);
	set.emplace(2'140'290'324'704);
	set.emplace(2'140'290'324'714);
	set.emplace(2'140'290'335'169);
	set.emplace(2'140'290'324'729);
	sprt::cout << (set.find(2'140'290'324'729) == set.end()) << "\n";
	set.erase(2'140'290'324'704);
	//set.erase(2'140'290'324'729);

	auto it = set.find(2'140'290'324'729);
	sprt::cout << (it == set.end()) << "\n";
	if (it == set.end()) {
		sprt::cout << "Failed\n";
	} else {
		sprt::cout << "Passed\n";
	}
}

static void edgecases3() {
	using unordered_set = __malloc_unordered_set<uintptr_t>;

	unordered_set set;
	set.max_load_factor(2.0f);

	set.emplace(2'140'661'643'422);
	set.emplace(2'140'661'643'426);
	set.emplace(2'140'661'643'430);
	set.emplace(2'140'661'643'433);
	set.emplace(2'140'661'643'437);
	set.emplace(2'140'661'643'441);
	set.emplace(2'140'661'643'445);
	set.emplace(2'140'661'643'448);
	set.emplace(2'140'661'643'452);
	set.emplace(2'140'661'643'456);
	set.emplace(2'140'661'643'460);
	set.emplace(2'142'809'137'023);
	set.emplace(2'143'882'871'293);
	set.emplace(2'143'882'871'305);
	set.emplace(2'143'882'871'317);
	set.emplace(2'140'661'643'482);
	set.emplace(2'142'809'137'033);
	set.emplace(2'143'882'872'829);
	set.emplace(2'142'809'137'038);
	set.emplace(2'142'809'137'673);
	set.emplace(2'143'882'873'603);
	set.emplace(2'142'809'138'313);
	set.emplace(2'143'882'874'371);
	set.emplace(2'142'809'138'318);
	set.emplace(2'143'882'874'383);
	set.emplace(2'142'809'138'323);
	set.emplace(2'142'809'138'328);
	set.emplace(2'142'809'137'678);
	set.emplace(2'143'882'873'615);
	set.emplace(2'142'809'137'683);
	set.emplace(2'142'809'137'688);
	set.emplace(2'142'809'138'953);
	set.emplace(2'143'882'875'139);
	set.emplace(2'142'809'138'958);
	set.emplace(2'143'882'875'151);
	set.emplace(2'142'809'138'963);
	set.emplace(2'142'809'138'968);
	set.emplace(2'143'882'871'401);
	set.emplace(2'140'661'643'497);
	set.erase(2'142'809'137'033);

	auto it = set.find(2'140'661'643'497);
	sprt::cout << (it == set.end()) << "\n";
	if (it == set.end()) {
		sprt::cout << "Failed\n";
	} else {
		sprt::cout << "Passed\n";
	}
}


void performMallocUnorderedSetTests() {
	using unordered_set = __malloc_unordered_set<int>;

	sprt::cout << "\n== unordered_set tests ==\n";

	edgecases1();
	edgecases2();
	edgecases3();

	// Test 1: Default constructor and empty check
	{
		unordered_set l;
		sprt::cout << "Test 1 - Default constructor: ";
		if (l.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 2: Size and max_size
	{
		unordered_set l;
		sprt::cout << "Test 2 - Size and max_size: ";
		if (l.empty() && l.size() == 0 && l.max_size() > 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 3: Copy constructor
	{
		unordered_set l1;
		l1.insert(1);
		l1.insert(2);

		unordered_set l2(l1);
		sprt::cout << "Test 3 - Copy constructor: ";
		if (l2.size() == 2 && l2.find(1) != l2.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 4: Move constructor
	{
		unordered_set l1;
		l1.insert(1);
		l1.insert(2);

		unordered_set l2(sprt::move(l1));
		sprt::cout << "Test 4 - Move constructor: ";
		if (l2.size() == 2 && l1.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 5: Assignment operator
	{
		unordered_set l1;
		l1.insert(1);

		unordered_set l2;
		l2 = l1;
		sprt::cout << "Test 5 - Assignment operator: ";
		if (l2.size() == 1 && l2.find(1) != l2.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 6: Move assignment operator
	{
		unordered_set l1;
		l1.insert(1);

		unordered_set l2;
		l2 = sprt::move(l1);
		sprt::cout << "Test 6 - Move assignment operator: ";
		if (l2.size() == 1 && l1.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 7: Insert with value_type
	{
		unordered_set l;
		auto result = l.insert(1);
		sprt::cout << "Test 7 - Insert with value_type: ";
		if (result.second && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 8: Insert with rvalue
	{
		unordered_set l;
		auto result = l.insert(2);
		sprt::cout << "Test 8 - Insert with rvalue: ";
		if (result.second && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 9: Insert range
	{
		unordered_set l;
		int arr[] = {1, 2};
		l.insert(arr, arr + 2);
		sprt::cout << "Test 9 - Insert range: ";
		if (l.size() == 2) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 10: Insert with initializer_list
	{
		unordered_set l;
		l.insert({1, 2});
		sprt::cout << "Test 10 - Insert with initializer_list: ";
		if (l.size() == 2) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 11: Emplace
	{
		unordered_set l;
		auto result = l.emplace(3);
		sprt::cout << "Test 11 - Emplace: ";
		if (result.second && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 13: Find
	{
		unordered_set l;
		l.insert(1);
		l.insert(2);
		l.insert(3);
		auto it = l.find(2);
		sprt::cout << "Test 13 - Find: ";
		if (it != l.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 14: Count
	{
		unordered_set l;
		l.insert(1);
		size_t count = l.count(1);
		sprt::cout << "Test 14 - Count: ";
		if (count == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 15: Contains
	{
		unordered_set l;
		l.insert(1);
		bool contains = l.contains(1);
		sprt::cout << "Test 15 - Contains: ";
		if (contains) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 16: Equal range
	{
		unordered_set l;
		l.insert(1);
		auto range = l.equal_range(1);
		sprt::cout << "Test 16 - Equal range: ";
		if (range.first != l.end() && range.second != l.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 17: Load factor and max load factor
	{
		unordered_set l;
		float load_factor = l.load_factor();
		float max_load_factor = l.max_load_factor();
		sprt::cout << "Test 17 - Load factor and max load factor: ";
		if (load_factor >= 0 && max_load_factor >= 1.0f) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 18: Rehash
	{
		unordered_set l;
		l.rehash(10);
		sprt::cout << "Test 18 - Rehash: ";
		if (l.size() >= 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 19: Clear
	{
		unordered_set l;
		l.insert(1);
		l.clear();
		sprt::cout << "Test 19 - Clear: ";
		if (l.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 20: Swap
	{
		unordered_set l1;
		l1.insert(1);

		unordered_set l2;
		l2.insert(2);

		l1.swap(l2);
		sprt::cout << "Test 20 - Swap: ";
		if (l1.size() == 1 && l2.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 21: Begin and end iterators
	{
		unordered_set l;
		l.insert(1);
		auto begin_it = l.begin();
		auto end_it = l.end();
		sprt::cout << "Test 21 - Begin and end iterators: ";
		if (begin_it != end_it) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 22: Begin and end const iterators
	{
		unordered_set l;
		l.insert(1);
		const unordered_set &cl = l;
		auto begin_it = cl.begin();
		auto end_it = cl.end();
		sprt::cout << "Test 22 - Begin and end const iterators: ";
		if (begin_it != end_it) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 23: Erase by position
	{
		unordered_set l;
		l.insert(1);
		l.insert(2);
		auto it = l.begin();
		l.erase(it);
		sprt::cout << "Test 23 - Erase by position: ";
		if (l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 24: Erase by key
	{
		unordered_set l;
		l.insert(1);
		l.insert(2);
		size_t erased = l.erase(1);
		sprt::cout << "Test 24 - Erase by key: ";
		if (erased == 1 && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 25: Iterator operations
	{
		unordered_set l;
		l.insert(1);
		l.insert(2);
		auto it = l.begin();
		++it; // Move to second element
		sprt::cout << "Test 25 - Iterator operations: ";
		if (it != l.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	sprt::cout << "\nUnordered set tests completed.\n";
}

} // namespace sprt
