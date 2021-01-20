#ifndef NET_TENSOR_CONTRACT_HPP
#define NET_TENSOR_CONTRACT_HPP
#include "network.hpp"
#include "rational.hpp"
#include "tensor_tools.hpp"
#include "traits.hpp"
#include <TAT/TAT.hpp>
#include <cstdlib>
#include <functional>
#include <random>
#include <variant>
#include <memory>
#include <vector>
#include <iostream>
#ifdef USE_LIB_KAHYPAR
	#include <libkahypar.h>
#else
	#include <kahypar/application/command_line_options.h>
	#include <kahypar/definitions.h>
	#include <kahypar/io/hypergraph_io.h>
	#include <kahypar/partitioner_facade.h>
#endif


namespace net {
	inline bool contract_test_mode = false;
	inline bool contract_trace_mode = false;
	class Engine {
	public:
		template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
		NodeVal contract(
				network<NodeVal, int, NodeKey, EdgeKey, Trait> &,
				const std::set<NodeKey, typename Trait::nodekey_less> &,
				const absorb_type &,
				const contract_type &);
		template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
		NodeVal contract_qbb(
				network<NodeVal, int, NodeKey, EdgeKey, Trait> &,
				const std::set<NodeKey, typename Trait::nodekey_less> &,
				const absorb_type &,
				const contract_type &);
		template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
		NodeVal contract_naive(
				network<NodeVal, int, NodeKey, EdgeKey, Trait> &,
				const std::set<NodeKey, typename Trait::nodekey_less> &,
				const absorb_type &,
				const contract_type &);

		int coarse_grain_to = 800;
		int cut_part = 8;
		int refine_sweep = 10000;
		int max_quickbb_size = 10;
		//double uneven = 0.2;
		std::default_random_engine rand = std::default_random_engine(0);
		// std::default_random_engine
		// rand=std::default_random_engine(std::random_device()());
	private:
		template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
		NodeKey inner_contract(
				network<NodeVal, int, NodeKey, EdgeKey, Trait> &,
				const std::set<NodeKey, typename Trait::nodekey_less> &,
				const absorb_type &,
				const contract_type &,
				double,bool);

		template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
		NodeKey inner_contract_usebetter(
				network<NodeVal, int, NodeKey, EdgeKey, Trait> &,
				const std::set<NodeKey, typename Trait::nodekey_less> &,
				const absorb_type &,
				const contract_type &);

		template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
		NodeKey contract_quickbb(
				network<NodeVal, int, NodeKey, EdgeKey, Trait> &,
				const std::set<NodeKey, typename Trait::nodekey_less> &,
				const absorb_type &,
				const contract_type &);

		template <typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
		std::vector<std::set<NodeKey, typename Trait::nodekey_less>>
		divide(network<NodeVal, int, NodeKey, EdgeKey, Trait> &, const std::set<NodeKey, typename Trait::nodekey_less> &);

		template <typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
		std::vector<std::set<NodeKey, typename Trait::nodekey_less>>
		divide_kahypar(network<NodeVal, int, NodeKey, EdgeKey, Trait> &, const std::set<NodeKey, typename Trait::nodekey_less> &,double);

		template <typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
		double refine(
				network<NodeVal, int, NodeKey, EdgeKey, Trait> &,
				const std::set<NodeKey, typename Trait::nodekey_less> &,
				std::vector<std::set<NodeKey, typename Trait::nodekey_less>> &);

		template <typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
		void adjust(
				network<NodeVal, int, NodeKey, EdgeKey, Trait> &,
				const std::set<NodeKey, typename Trait::nodekey_less> &,
				std::vector<std::set<NodeKey, typename Trait::nodekey_less>> &,
				double);
	};

	template <typename contract_type>
	struct lift_contract {
		contract_type contract_fun;
		lift_contract(contract_type cf) : contract_fun(cf){};
		template <typename NodeVal, typename NoUse>
		NodeVal operator()(const NodeVal & ten1, const NodeVal & ten2, const NoUse & inds) const {
			return std::make_tuple(contract_fun(std::get<0>(ten1), std::get<0>(ten2), inds), std::get<1>(ten1), std::get<2>(ten1));
		}
	};

	template <typename absorb_type>
	struct lift_absorb {
		absorb_type absorb_fun;
		lift_absorb(absorb_type af) : absorb_fun(af){};
		template <typename NodeVal, typename EdgeVal, typename NoUse>
		NodeVal operator()(const NodeVal & ten1, const EdgeVal & eg, const NoUse & ind) const {
			return std::make_tuple(absorb_fun(std::get<0>(ten1), eg, ind), std::get<1>(ten1), std::get<2>(ten1));
		}
	};

	struct kset_contract {
		template <typename NodeVal, typename EdgeKey, typename Comp>
		NodeVal operator()(const NodeVal & g1, const NodeVal & g2, const std::set<std::pair<EdgeKey, EdgeKey>, Comp> & inds) const {
			NodeVal res = g1;
			res.insert(g2.begin(), g2.end());
			return res;
		}
	};

	template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	NodeVal Engine::contract(
			network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat,
			const std::set<NodeKey, typename Trait::nodekey_less> & part,
			const absorb_type & absorb_fun,
			const contract_type & contract_fun) {
		network<std::tuple<NodeVal, int, net::rational>, int, NodeKey, EdgeKey, Trait> temp;
		temp = lat.template fmap<decltype(temp)>(
				[](const NodeVal & ten) { return std::make_tuple(ten, 0, net::rational(0, 1)); }, [](const int & m) { return m; });
		std::string final_site = inner_contract(temp, part, lift_absorb(absorb_fun), lift_contract(contract_fun),0.,false);
		return std::get<0>(temp[final_site].val);
	}
	template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	NodeVal Engine::contract_qbb(
			network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat,
			const std::set<NodeKey, typename Trait::nodekey_less> & part,
			const absorb_type & absorb_fun,
			const contract_type & contract_fun) {
		network<std::tuple<NodeVal, int, net::rational>, int, NodeKey, EdgeKey, Trait> temp;
		temp = lat.template fmap<decltype(temp)>(
				[](const NodeVal & ten) { return std::make_tuple(ten, 0, net::rational(0, 1)); }, [](const int & m) { return m; });
		std::string final_site = contract_quickbb(temp, part, lift_absorb(absorb_fun), lift_contract(contract_fun));
		return std::get<0>(temp[final_site].val);
	}
	template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	NodeVal Engine::contract_naive(
			network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat,
			const std::set<NodeKey, typename Trait::nodekey_less> & part,
			const absorb_type & absorb_fun,
			const contract_type & contract_fun) {
		network<std::tuple<NodeVal, int, net::rational>, int, NodeKey, EdgeKey, Trait> temp;
		temp = lat.template fmap<decltype(temp)>(
				[](const NodeVal & ten) { return std::make_tuple(ten, 0, net::rational(0, 1)); }, [](const int & m) { return m; });
		return std::get<0>(temp.contract(part,lift_absorb(absorb_fun), lift_contract(contract_fun)));
	}

	template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	NodeKey Engine::inner_contract(
			network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat,
			const std::set<NodeKey, typename Trait::nodekey_less> & part,
			const absorb_type & absorb_fun,
			const contract_type & contract_fun,double uneven, bool fix_uneven) {

		if (part.size() > max_quickbb_size) {
			//std::vector<std::set<NodeKey, typename Trait::nodekey_less>> subparts = divide(lat, part);
			std::vector<std::set<NodeKey, typename Trait::nodekey_less>> subparts = divide_kahypar(lat, part);
			std::set<NodeKey, typename Trait::nodekey_less> new_part;
			for (auto & p : subparts)
				new_part.insert(inner_contract(lat, p, absorb_fun, contract_fun));
			return inner_contract(lat, new_part, absorb_fun, contract_fun);
			// for(auto & p:subparts)
			// 	new_part.insert(contract_quickbb<contract_type>(lat,p));
			// return contract_quickbb<contract_type>(lat,new_part);
		} else {
			return contract_quickbb(lat, part, absorb_fun, contract_fun);
		}
	}

	template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	NodeKey Engine::inner_contract_usebetter(
			network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat,
			const std::set<NodeKey, typename Trait::nodekey_less> & part,
			const absorb_type & absorb_fun,
			const contract_type & contract_fun) {

		NodeKey contract_res1,contract_res2;
		
		double new_eneven,best_eneven,min_cost,this_cost;
		//network<NodeVal, int, NodeKey, EdgeKey, Trait>  temp2 = lat;

		if (part.size() > max_quickbb_size) {
			if(!fix_uneven){
				min_cost=-1.;
				for(int j=0;j<50;++j){
					NodeKey contract_res1;
					network<NodeVal, int, NodeKey, EdgeKey, Trait>  temp = lat;
					new_eneven=0.02*j;
					//std::cout<<"part = "<<eg.cut_part<<" uneven = "<<eg.uneven<<'\n';
					std::vector<std::set<NodeKey, typename Trait::nodekey_less>> subparts = divide_kahypar(temp, part,new_eneven);
					std::set<NodeKey, typename Trait::nodekey_less> new_part;
					for (auto & p : subparts)
						new_part.insert(inner_contract(temp, p, absorb_fun, contract_fun,new_eneven,true));
					contract_res1=inner_contract(temp, new_part, absorb_fun, contract_fun,new_eneven,true);
					//std::cout<<' '<<ctree->val.hist_max_weight<<','<<ctree->val.contraction_cost<<'\n';
					this_cost=std::get<0>(temp[contract_res1].val)->val.contraction_cost;
					if(min_cost<0 || this_cost<min_cost){
						min_cost=this_cost;
						best_eneven=new_eneven;
					}
					
				}
				std::vector<std::set<NodeKey, typename Trait::nodekey_less>> subparts = divide_kahypar(lat, part,best_eneven);
				std::set<NodeKey, typename Trait::nodekey_less> new_part;
				for (auto & p : subparts)
					new_part.insert(inner_contract(lat, p, absorb_fun, contract_fun,best_eneven,false));
				return inner_contract(lat, new_part, absorb_fun, contract_fun,best_eneven,true);

			}else{
				std::vector<std::set<NodeKey, typename Trait::nodekey_less>> subparts = divide_kahypar(lat, part,uneven);
				std::set<NodeKey, typename Trait::nodekey_less> new_part;
				for (auto & p : subparts)
					new_part.insert(inner_contract(lat, p, absorb_fun, contract_fun,uneven,true));
				return inner_contract(lat, new_part, absorb_fun, contract_fun,uneven,true);

			}

		} else {
			return contract_quickbb(lat, part, absorb_fun, contract_fun);
		}
	}

	// template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	// NodeKey Engine::inner_contract(
	// 		network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat,
	// 		const std::set<NodeKey, typename Trait::nodekey_less> & part,
	// 		const absorb_type & absorb_fun,
	// 		const contract_type & contract_fun) {

	// 	NodeKey contract_res1,contract_res2;
	// 	network<NodeVal, int, NodeKey, EdgeKey, Trait>  temp1 = lat;
	// 	network<NodeVal, int, NodeKey, EdgeKey, Trait>  temp2 = lat;

	// 	if (part.size() > max_quickbb_size) {
	// 		//std::vector<std::set<NodeKey, typename Trait::nodekey_less>> subparts = divide(lat, part);
	// 		std::vector<std::set<NodeKey, typename Trait::nodekey_less>> subparts = divide_kahypar(temp1, part);
	// 		std::set<NodeKey, typename Trait::nodekey_less> new_part;
	// 		for (auto & p : subparts)
	// 			new_part.insert(inner_contract(temp1, p, absorb_fun, contract_fun));
	// 		contract_res1=inner_contract(temp1, new_part, absorb_fun, contract_fun);
	// 		//std::cout<<"<contract_chose\n"<<temp2.size();
	// 		contract_res2=contract_quickbb(temp2, part, absorb_fun, contract_fun);

	// 		//std::cout<<"\n"<<std::get<0>(temp1[contract_res1].val)->val.contraction_cost<<' '<<
	// 		//	std::get<0>(temp2[contract_res2].val)->val.contraction_cost<<"\n"<<part.size()<<"\ncontract_chose>\n";

	// 		if(std::get<0>(temp1[contract_res1].val)->val.contraction_cost < std::get<0>(temp2[contract_res2].val)->val.contraction_cost){
	// 			lat=temp1;
	// 			return contract_res1;
	// 		}else{
	// 			lat=temp2;
	// 			return contract_res2;
	// 		}
	// 		// for(auto & p:subparts)
	// 		// 	new_part.insert(contract_quickbb<contract_type>(lat,p));
	// 		// return contract_quickbb<contract_type>(lat,new_part);
	// 	} else {
	// 		return contract_quickbb(lat, part, absorb_fun, contract_fun);
	// 	}
	// }

	// find neighbor with least contraction count of a node within given part
	// EdgeVal = int
	template <typename IterNode, typename NodeSet, typename EdgeKey, typename Trait>
	std::pair<std::pair<int,int>, IterNode> search_quick(IterNode & it1, const NodeSet & part) {
		std::map<EdgeKey, std::pair<std::pair<int,int>, IterNode>, typename Trait::edgekey_less> legs;
		for (auto & e : it1->second.edges) {
			if (part.count(e.second.nbkey) > 0) {
				if (legs.count(e.second.nbkey) == 0)
					legs[e.second.nbkey] = 
					{{std::get<1>(it1->second.val) * std::get<1>(e.second.nbitr->second.val) / e.second.val,
					std::get<1>(it1->second.val) * std::get<1>(e.second.nbitr->second.val) / e.second.val / e.second.val},
					e.second.nbitr};
				else{
					legs[e.second.nbkey].first.first /= e.second.val;
					legs[e.second.nbkey].first.second /= (e.second.val*e.second.val);
				}
			}
		}
		auto min_itr = std::min_element(
				legs.begin(),
				legs.end(),
				[](const std::pair<EdgeKey, std::pair<std::pair<int,int>, IterNode>> & a, const std::pair<EdgeKey, std::pair<std::pair<int,int>, IterNode>> & b) {
					return a.second.first < b.second.first;
				});
		return min_itr->second;
	}

	template <typename contract_type, typename absorb_type, typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	NodeKey Engine::contract_quickbb(
			network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat,
			const std::set<NodeKey, typename Trait::nodekey_less> & part,
			const absorb_type & absorb_fun,
			const contract_type & contract_fun) {
		// if(contract_test_mode)
		//	lat.draw("lat before quickbb",{part},true);

		auto temp=lat;
		//std::cout<<lat.size()<<' '<<temp.size();
		std::set<NodeKey, typename Trait::nodekey_less> contracted_sites;

		if (contract_trace_mode)
			std::cout << "in_quickbb \n";
		if (part.size() == 1){
			return *(part.begin());
		}
		// else if(part.size() == 2){
		// 	lat.absorb(*(part.begin()), *(part.begin()++), absorb_fun, contract_fun);
		// 	return *(part.begin());
		// }

		// std::cout << "<test_part \n";
		// for(auto & p:part) std::cout<<p<<'\n';
		// std::cout << "test_part> \n";

		// lat.draw("part",{part},true);

		std::pair<int,int> count;
		std::pair<int,int> least_count = {-1,-1};
		using NodeItrType = typename network<NodeVal, int, NodeKey, EdgeKey, Trait>::IterNode;
		using KeySet = std::set<NodeKey, typename Trait::nodekey_less>;
		NodeItrType least_contract1, least_contract2, nb_itr;

		for (auto & p : part) {
			auto site_it = lat.find(p);
			std::get<1>(site_it->second.val) = calc_weight(site_it, lat, KeySet());
		}
		for (auto & p : part) {
			auto site_it = lat.find(p);
			std::tie(count, nb_itr) = search_quick<NodeItrType, KeySet, EdgeKey, Trait>(site_it, part);
			if (least_count < std::make_pair(0,0) || count < least_count) {
				least_count = count;
				least_contract1 = site_it;
				least_contract2 = nb_itr;
			}
		}
		int contract_size;
		auto root_itr = least_contract1;
		lat.absorb(root_itr, least_contract2, absorb_fun, contract_fun);
		contracted_sites.insert(root_itr->first);
		contracted_sites.insert(least_contract2->first);
		//temp.draw("quickbb",{contracted_sites},false);
		contract_size = 2;
		while (contract_size < part.size()) {
			std::tie(count, nb_itr) = search_quick<NodeItrType, KeySet, EdgeKey, Trait>(root_itr, part);
			lat.absorb(root_itr, nb_itr, absorb_fun, contract_fun);
			contracted_sites.insert(nb_itr->first);
			//temp.draw("quickbb",{contracted_sites},false);
			++contract_size;
		}

		if (contract_trace_mode)
			std::cout << "out_quickbb \n";
		return root_itr->first;
	}

	template <typename IterNode, typename NodeSet1, typename NodeSet2>
	int calc_weight(const IterNode & it, const NodeSet1 & includes, const NodeSet2 & excludes) {
		int weight = 1;
		for (auto & e : it->second.edges)
			if (includes.count(e.second.nbkey) > 0 && excludes.count(e.second.nbkey) == 0)
				weight *= e.second.val;
		return weight;
	}

	template <typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	void combine_edges(network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat, const std::set<NodeKey, typename Trait::nodekey_less> & includes) {
		for (auto & i : includes) {
			auto & inode = lat[i];
			std::map<NodeKey, std::pair<EdgeKey, EdgeKey>, typename Trait::nodekey_less> nbkey2ind;
			for (auto iter = inode.edges.begin(); iter != inode.edges.end();) {
				// std::cout<<"combine "<<i<<' '<<iter->first<<'
				// '<<iter->second.nbkey<<'\n';
				if (nbkey2ind.count(iter->second.nbkey) == 0) {
					nbkey2ind.insert({iter->second.nbkey, {iter->first, iter->second.nbind}});
					++iter;
				} else {
					// std::cout<<"combine_erase "<<i<<' '<<iter->first<<'
					// '<<iter->second.nbkey<<'\n';
					inode.edges[nbkey2ind[iter->second.nbkey].first].val *= iter->second.val;
					iter->second.nbitr->second.edges[nbkey2ind[iter->second.nbkey].second].val *= iter->second.nbitr->second.edges[iter->second.nbind].val;
					iter->second.nbitr->second.edges.erase(iter->second.nbind);
					iter = inode.edges.erase(iter);
				}
			}
		}
	}

	template <typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	void get_component(
			node<NodeVal, int, NodeKey, EdgeKey, Trait> & n,
			const NodeKey & p,
			const std::set<NodeKey, typename Trait::nodekey_less> & part,
			std::set<NodeKey, typename Trait::nodekey_less> & treated,
			std::set<NodeKey, typename Trait::nodekey_less> & component) {
		treated.insert(p);
		component.insert(p);
		for (auto & eg : n.edges)
			if (part.count(eg.second.nbkey) > 0 && treated.count(eg.second.nbkey) == 0 && std::get<1>(n.val) == std::get<1>(eg.second.nbitr->second.val))
				get_component(eg.second.nbitr->second, eg.second.nbkey, part, treated, component);
	}

	template <typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	std::vector<std::set<NodeKey, typename Trait::nodekey_less>> disconnect(
			network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat,
			const std::set<NodeKey, typename Trait::nodekey_less> & part,
			const std::vector<std::set<NodeKey, typename Trait::nodekey_less>> & subparts) {
		for (int i = 0; i < subparts.size(); ++i)
			for (auto & s : subparts[i])
				std::get<1>(lat[s].val) = i;

		std::set<NodeKey, typename Trait::nodekey_less> treated, newsubpart;
		std::vector<std::set<NodeKey, typename Trait::nodekey_less>> newsubparts;
		while (treated.size() < part.size())
			for (auto & p : part)
				if (treated.count(p) == 0) {
					newsubparts.push_back({});
					get_component(lat[p], p, part, treated, newsubparts.back());
				}

		return newsubparts;
	}

	template <typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	std::vector<std::set<NodeKey, typename Trait::nodekey_less>>
	Engine::divide_kahypar(network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat, const std::set<NodeKey, typename Trait::nodekey_less> & part, double uneven) {

		const kahypar_hypernode_id_t num_vertices = part.size();
		kahypar_hyperedge_id_t num_hyperedges = 0;

		std::vector<kahypar_hyperedge_weight_t> hyperedge_weights; // weight of edge

		std::vector<size_t> hyperedge_indices; // start and end of vertice list for each edge

		std::vector<kahypar_hyperedge_id_t> hyperedges; // vertice list

		std::set<NodeKey, typename Trait::nodekey_less> treated_nodes;

		std::map<NodeKey,int, typename Trait::nodekey_less> site_id_map;
		std::vector<NodeKey> inv_site_id_map;

		//std::cout<<"in3\n";
		int site_id=0;
		for (auto & p : part){
			inv_site_id_map.push_back(p);
			site_id_map[p] = site_id++;
		}

		//std::cout<<"in4\n";
		int edge_id=0;
		hyperedge_indices.push_back(0);
		for (auto & s_it : lat) {
			auto & nodekey1 = s_it.first;
			for (auto & b_it : s_it.second.edges) {
				auto & nodekey2 = b_it.second.nbkey;
				if (treated_nodes.count(nodekey2) == 0) {
					if (part.count(nodekey1) == 1 && part.count(nodekey2) == 1){
		//std::cout<<"in5\n";
						hyperedge_weights.push_back(std::log10(double(b_it.second.val))*40);
						// fail if not *10. maybe too small. demand double check.

		//std::cout<<"in6\n";
						hyperedge_indices.push_back(2*(edge_id+1));
		//std::cout<<"in7\n";
						hyperedges.push_back(site_id_map[nodekey1]);
						hyperedges.push_back(site_id_map[nodekey2]);
						edge_id++;
						num_hyperedges++;
		//std::cout<<"in8\n";
					}
				}
			}
			treated_nodes.insert(nodekey1);
		}

		//std::cout<<"in5\n";
			
		const double imbalance = uneven;
		const kahypar_partition_id_t k = 2;
			
		kahypar_hyperedge_weight_t objective = 0;

		std::vector<kahypar_partition_id_t> partition(num_vertices, -1);

#ifdef USE_LIB_KAHYPAR
		kahypar_context_t* context = kahypar_context_new();
		kahypar_configure_context_from_file(context, "km1_kKaHyPar_sea20.ini");
		kahypar_partition(num_vertices, num_hyperedges,
		   	            imbalance, k,
		           	    /*vertex_weights */ nullptr, hyperedge_weights.data(),
		           	    hyperedge_indices.data(), hyperedges.data(),
		   	            &objective, context, partition.data());
		kahypar_context_free(context);
#else
		kahypar::Context& context;
		kahypar::parseIniToContext(context,"km1_kKaHyPar_sea20.ini");
		ASSERT(!context.partition.use_individual_part_weights ||
		     !context.partition.max_part_weights.empty());
		ASSERT(partition != nullptr);

		context.partition.k = k;
		context.partition.epsilon = imbalance;
		context.partition.write_partition_file = false;

		kahypar::Hypergraph hypergraph(num_vertices,
		                             num_hyperedges,
		                             hyperedge_indices.data(),
		                             hyperedges.data(),
		                             context.partition.k,
		                             hyperedge_weights.data(),
		                             /*vertex_weights */ nullptr);

		if (context.partition.vcycle_refinement_for_input_partition) {
			for (const auto hn : hypergraph.nodes()) {
				hypergraph.setNodePart(hn, partition[hn]);
			}
		}

		kahypar::PartitionerFacade().partition(hypergraph, context);

		*objective = kahypar::metrics::correctMetric(hypergraph, context);

		for (const auto hn : hypergraph.nodes()) {
			partition[hn] = hypergraph.partID(hn);
		}

		context.partition.perfect_balance_part_weights.clear();
		context.partition.max_part_weights.clear();
		context.evolutionary.communities.clear();
#endif

		std::vector<std::set<NodeKey, typename Trait::nodekey_less>> results(k);
		//std::cout<<"result\n";
		for(int i = 0; i != num_vertices; ++i){
		//	std::cout<<i<<' '<<partition[i]<<' '<<inv_site_id_map[i]<<'\n';
			results[partition[i]].insert(inv_site_id_map[i]);
		}
		//std::cout<<"part\n";
		//for(auto &p :part)
		//	std::cout<<p<<'\n';

		//return results;
		return disconnect(lat, part, results);
	}

	template <typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	std::vector<std::set<NodeKey, typename Trait::nodekey_less>>
	Engine::divide(network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat, const std::set<NodeKey, typename Trait::nodekey_less> & part) {
		if (contract_trace_mode)
			std::cout << "in_divide \n";
		using KeySet = std::set<NodeKey, typename Trait::nodekey_less>;
		KeySet coarse_part = part;
		network<KeySet, int, NodeKey, EdgeKey> fakelat;
		fakelat = lat.template fmap<decltype(fakelat)>([](const NodeVal & tp) { return KeySet(); }, [](const int & m) { return m; });
		for (auto & n : fakelat)
			n.second.val.insert(n.first);

		//粗粒化
		// for(auto & i: std::get<0>(lat["ten0_1"])->val ) std::cout<<i<<"
		// **************1\n"; 	std::cout<<"finish **************1\n";

		if (contract_trace_mode)
			std::cout << "coarse_grain \n";
		combine_edges(fakelat, part);
		while (coarse_part.size() > coarse_grain_to) {
			std::set<std::tuple<int, NodeKey, NodeKey>, std::greater<std::tuple<int, NodeKey, NodeKey>>> ordered_bond; // weight, from, to
																																						  // we may replace std::greater
			// add bonds to ordered_bond
			for (auto & p : coarse_part) {
				auto & this_site = fakelat[p];
				for (auto & e : this_site.edges) {
					if (coarse_part.count(e.second.nbkey) > 0) {
						ordered_bond.insert({e.second.val, p, e.second.nbkey});
					}
				}
			}
			// do coarse grain
			KeySet treated_sites;
			for (auto & b : ordered_bond) {
				if (treated_sites.count(std::get<1>(b)) == 0 && treated_sites.count(std::get<2>(b)) == 0) {
					treated_sites.insert(std::get<1>(b));
					treated_sites.insert(std::get<2>(b));
					fakelat.absorb(std::get<1>(b), std::get<2>(b), no_absorb(), kset_contract());
					coarse_part.erase(std::get<2>(b));
					combine_edges(fakelat, {std::get<1>(b)});
				}
			}
		}
		// lat.draw(true);
		// for(auto & i: std::get<0>(lat["ten0_1"])->val ) std::cout<<i<<"
		// **************2\n"; 	std::cout<<"finish **************2\n";

		if (contract_trace_mode)
			std::cout << "initial \n";
		//初始分割，分成cut_part份
		KeySet treated_sites = {};
		KeySet final_sites = {};
		int size_limit;
		int treated_size = 0;
		bool decide2exit;
		for (int i = 0; i < cut_part; ++i) {
			size_limit = (part.size() - treated_size) / (cut_part - i);
			if (contract_test_mode)
				fakelat.draw("lat before initial_cut no. " + std::to_string(i), {part, final_sites}, true);
			// find site with max weight (within part-treated)
			int max_weight = 0;
			int this_weight;
			typename network<KeySet, int, NodeKey, EdgeKey>::IterNode max_weight_site_itr;
			typename network<KeySet, int, NodeKey, EdgeKey>::IterNode this_site_itr;
			for (auto p : coarse_part) {
				if (treated_sites.count(p) == 0) {
					this_site_itr = fakelat.find(p);
					this_weight = calc_weight(this_site_itr, coarse_part, treated_sites);
					if (contract_test_mode)
						std::cout << "test.divide.initial_cut.max_weight " << p << ' ' << this_weight << '\n';
					if (this_weight > max_weight) {
						max_weight = this_weight;
						max_weight_site_itr = this_site_itr;
					}
				}
			}
			if (contract_test_mode)
				std::cout << "test.divide.initial_cut.max_weight final " << max_weight_site_itr->first << ' ' << max_weight << '\n';
			// construct subpart
			treated_sites.insert(max_weight_site_itr->first);
			final_sites.insert(max_weight_site_itr->first);
			while (true) {
				// std::cout<<"a \n";
				// fakelat.consistency();
				combine_edges(fakelat, {max_weight_site_itr->first});
				// std::cout<<"b \n";
				// fakelat.consistency();
				std::set<std::pair<int, NodeKey>, std::greater<std::pair<int, NodeKey>>> ordered_nb;
				for (auto & e : max_weight_site_itr->second.edges) {
					if (coarse_part.count(e.second.nbkey) > 0 && treated_sites.count(e.second.nbkey) == 0) {
						ordered_nb.insert({e.second.val, e.second.nbkey});
					}
				}
				if (ordered_nb.size() == 0) // exit1: no neighbors
					break;
				decide2exit = false;
				for (auto & nb : ordered_nb) {
					if (max_weight_site_itr->second.val.size() + fakelat[nb.second].val.size() <= size_limit) {
						fakelat.absorb(max_weight_site_itr->first, nb.second, no_absorb(), kset_contract());
						treated_sites.insert(nb.second);
					} else {
						decide2exit = true; // exit2: reach limit
						break;
					}
				}
				if (decide2exit)
					break;
			}

			treated_size += max_weight_site_itr->second.val.size();
			// std::cout<<"c \n";
			// fakelat.consistency();
			combine_edges(fakelat, {max_weight_site_itr->first});
			// std::cout<<"d \n";
			// fakelat.consistency();
		}

		// if(contract_test_mode)
		// fakelat.draw("lat after initial_cut",{coarse_part,final_sites},true);
		if (contract_trace_mode)
			std::cout << "after_initial \n";
		//可能出现这种情况：由于连通性的原因，所有subpart搞完后，还剩下一些sites

		// int n=0;
		while (treated_sites.size() < coarse_part.size()) {
			// for(auto & e:fakelat.nodes.at("ten3_4").edges)
			// std::cout<<"hedddddre2 "<<e.second.nbkey<<'\n';
			// std::cout<<"diff"<<treated_sites.size()<<' '<<coarse_part.size()<<'\n';
			for (auto & s : final_sites) {
				// std::cout<<"here "<<s<<'\n';
				// auto & test_node=fakelat.nodes.at(s);
				// std::cout<<"there \n";
				// std::cout<<test_node.edges.size()<<"\n";
				// std::cout<<"there \n";
				for (auto & e : fakelat.at(s).edges) {
					// std::cout<<"here2
					// "<<e.second.nbkey<<coarse_part.count(e.second.nbkey)<<final_sites.count(e.second.nbkey)<<'\n';
					if (coarse_part.count(e.second.nbkey) > 0 && treated_sites.count(e.second.nbkey) == 0) {
						treated_sites.insert(e.second.nbkey);
						fakelat.absorb(s, e.second.nbkey, no_absorb(), kset_contract());
						// std::cout<<treated_sites.size()<<"success\n";
						// std::cout<<treated_sites.size()<<"success\n";
						break;
					}
				}
			}
			// n++;
			// if(n==10) std::exit(EXIT_FAILURE);
		}
		if (contract_test_mode)
			fakelat.draw("lat after adjustment", {part, final_sites}, true);

		if (contract_trace_mode)
			std::cout << "release \n";
		// for(auto & i: std::get<0>(lat["ten0_1"])->val ) std::cout<<i<<"
		// **************3\n"; 	std::cout<<"finish **************3\n";
		//释放
		std::vector<KeySet> subparts;
		for (auto & s : final_sites)
			subparts.push_back(fakelat[s].val);

		// for(auto & i: std::get<0>(lat["ten0_1"])->val ) std::cout<<i<<"
		// **************4\n"; 	std::cout<<"finish **************4\n";

		if (contract_trace_mode)
			std::cout << "out_divide \n";

		// adjust(lat,part,subparts,0.1);
		// refine(lat,part,subparts);
		adjust(lat, part, subparts, 0.3);
		// refine(lat,part,subparts);
		// adjust(lat,part,subparts,1);
		// refine(lat,part,subparts);
		if (contract_test_mode)
			lat.draw("lat after adjust", subparts, true);
		auto subparts2 = disconnect(lat, part, subparts);
		if (contract_test_mode)
			lat.draw("lat after disconnect", subparts2, true);
		return subparts2;
	}

	template <typename NodeType, typename SetType>
	bool calc_gain(int cut_part, net::rational & max_gain, NodeType & n, int & this_part, int & nb_part, const SetType & part) {
		std::vector<int> weights(cut_part, 1);
		for (auto & eg : n.edges)
			if (part.count(eg.second.nbkey) > 0)
				weights[std::get<1>(eg.second.nbitr->second.val)] *= eg.second.val;

		this_part = std::get<1>(n.val);
		nb_part = this_part;
		int this_weight = weights[this_part];
		int max_weight = this_weight;
		for (int i = 0; i < cut_part; i++) {
			if (weights[i] > max_weight) {
				max_weight = weights[i];
				nb_part = i;
			}
		}
		max_gain = net::rational(max_weight, this_weight);
		return (max_weight > this_weight);
	}

	template <typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	double Engine::refine(
			network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat,
			const std::set<NodeKey, typename Trait::nodekey_less> & part,
			std::vector<std::set<NodeKey, typename Trait::nodekey_less>> & subparts) {

		double uneven=0.;
		// if(contract_trace_mode) std::cout<<"in_refine \n";

		// for(auto & i: std::get<0>(lat["ten0_1"])->val ) std::cout<<i<<"
		// **************5\n"; 	std::cout<<"finish **************5\n";
		// if(contract_test_mode) lat.draw("lat before refinement",subparts,true);

		// set part label and  calculate part size
		std::vector<int> part_size(subparts.size(), 0);
		for (int i = 0; i < subparts.size(); ++i) {
			for (auto & s : subparts[i])
				std::get<1>(lat[s].val) = i;
			part_size[i] = subparts[i].size();
		}

		// calc gain sorted set
		std::map<std::pair<net::rational, NodeKey>, std::pair<int, int>, std::greater<std::pair<net::rational, NodeKey>>> gain_rec; // gain, from, to
		net::rational gain;
		double tot_gain = 1.;
		int this_part, nb_part;
		for (auto & p : part) {
			auto & n = lat[p];

			if (calc_gain(cut_part, gain, n, this_part, nb_part, part)) {
				gain_rec[{gain, p}] = {this_part, nb_part};
				std::get<2>(n.val) = gain;
			}
		}
		if (contract_test_mode) {
			std::cout << "test.refine.build_gain start\n";
			for (auto & i : gain_rec)
				std::cout << "test.refine.build_gain " << i.first.second << ' ' << i.first.first << ' ' << i.second.first << " -> " << i.second.second
							 << "\n";
			std::cout << "test.refine.build_gain finish\n";
		}

		// for(int i=0;i<subparts.size();++i){
		// 	std::cout<<"test.refine.run_gain subpart"<<i<<'\n';
		// 	for(auto & s:subparts[i]) std::cout<<"         "<<s<<'\n';
		// 	for(auto & s:subparts[i]) std::cout<<"         \n";
		// }

		int min_size = part.size() / cut_part * (1 - uneven);
		int max_size = part.size() / cut_part * (1 + uneven);
		for (int i = 0; i < refine_sweep; i++) {
			if (gain_rec.size() == 0)
				break;
			for (auto g_rec = gain_rec.begin(); g_rec != gain_rec.end(); ++g_rec) {
				if (part_size[g_rec->second.first] > min_size && part_size[g_rec->second.second] < max_size) {
					// std::cout<<"test.refine.run_gain "<<g_rec->first.second<<' '<<
					// 	g_rec->first.first<<' '<<g_rec->second.first<<" ->
					// "<<g_rec->second.second<<"\n";

					part_size[g_rec->second.first]--;
					part_size[g_rec->second.second]++;
					auto this_name = g_rec->first.second;
					auto & this_node = lat[this_name];
					std::get<1>(this_node.val) = g_rec->second.second;
					subparts[g_rec->second.first].erase(g_rec->first.second);
					subparts[g_rec->second.second].insert(g_rec->first.second);
					tot_gain *= g_rec->first.first.to_double();

					gain_rec.erase(g_rec);
					if (calc_gain(cut_part, gain, this_node, this_part, nb_part, part)) {
						gain_rec[{gain, this_name}] = {this_part, nb_part};
						std::get<2>(this_node.val) = gain;
					}
					for (auto & eg : this_node.edges) {
						if (part.count(eg.second.nbkey) > 0) {
							auto lookup_nb = gain_rec.find({std::get<2>(eg.second.nbitr->second.val), eg.second.nbkey});
							if (lookup_nb != gain_rec.end()) {
								gain_rec.erase(lookup_nb);
							}
							if (calc_gain(cut_part, gain, eg.second.nbitr->second, this_part, nb_part, part)) {
								gain_rec[{gain, eg.second.nbkey}] = {this_part, nb_part};
								std::get<2>(eg.second.nbitr->second.val) = gain;
							}
						}
					}
					break;
				}
			}
		}

		// for(auto & i: std::get<0>(lat["ten0_1"])->val ) std::cout<<i<<"
		// **************6\n"; 	std::cout<<"finish **************6\n";

		// if(contract_test_mode) lat.draw("lat after refinement",subparts,true);

		// if(contract_trace_mode) std::cout<<"out_refine \n";

		return tot_gain;
	}

	// calc weight of a node by part no.

	template <typename NodeType, typename SetType>
	void calc_weight(int cut_part, const NodeType & n, std::vector<int> & weight, const SetType & part) {
		weight = std::vector<int>(cut_part, 1);
		for (auto & eg : n.edges)
			if (part.count(eg.second.nbkey) > 0)
				weight[std::get<1>(eg.second.nbitr->second.val)] *= eg.second.val;
	}

	template <typename NodeVal, typename NodeKey, typename EdgeKey, typename Trait>
	void Engine::adjust(
			network<NodeVal, int, NodeKey, EdgeKey, Trait> & lat,
			const std::set<NodeKey, typename Trait::nodekey_less> & part,
			std::vector<std::set<NodeKey, typename Trait::nodekey_less>> & subparts,
			double alpha) {

		double uneven=0.;

		if (contract_trace_mode)
			std::cout << "in_adjust \n";
		std::vector<std::set<NodeKey, typename Trait::nodekey_less>> best_subparts = subparts, temp_subparts;
		std::vector<NodeKey> part_vec = {part.begin(), part.end()};

		// for(auto & i: std::get<0>(lat["ten0_1"])->val ) std::cout<<i<<"
		// **************5\n"; 	std::cout<<"finish **************5\n";
		if (contract_test_mode)
			lat.draw("lat before adjustment", subparts, true);

		// set part label and  calculate part size
		std::vector<int> part_size(subparts.size(), 0);
		for (int i = 0; i < subparts.size(); ++i) {
			for (auto & s : subparts[i])
				std::get<1>(lat[s].val) = i;
			part_size[i] = subparts[i].size();
		}

		// calc gain sorted set

		int min_size = part.size() / cut_part * (1 - 0.5 * uneven);
		int max_size = part.size() / cut_part * (1 + 0.5 * uneven);

		std::uniform_int_distribution<> site_dist(0, part.size() - 1);
		std::uniform_int_distribution<> part_dist(0, cut_part - 2);
		std::uniform_real_distribution<> monte_carlo;

		std::map<NodeKey, std::vector<int>, typename Trait::nodekey_less> weights;
		for (auto & p : part)
			calc_weight(cut_part, lat[p], weights[p], part);

		double cumu_gain = 1, max_gain = 1, further_gain;
		int i = 0;
		int j = 0;
		while (i < refine_sweep && j < 100 * refine_sweep) {
			j++;
			auto this_site = part_vec[site_dist(rand)];
			auto & this_node = lat[this_site];
			auto & this_weight = weights[this_site];
			int this_part = std::get<1>(this_node.val);
			int next_part = part_dist(rand);
			if (next_part >= this_part)
				next_part++;
			// std::cout<<std::pow(double(this_weight[next_part])/double(this_weight[this_part]),alpha)<<'\n';
			if (part_size[this_part] > min_size && part_size[next_part] < max_size &&
				 monte_carlo(rand) < std::pow(double(this_weight[next_part]) / double(this_weight[this_part]), alpha)) {
				// std::cout<<"success";
				cumu_gain *= double(this_weight[next_part]) / this_weight[this_part];

				part_size[this_part]--;
				part_size[next_part]++;
				std::get<1>(this_node.val) = next_part;
				subparts[this_part].erase(this_site);
				subparts[next_part].insert(this_site);
				calc_weight(cut_part, this_node, this_weight, part);
				for (auto & eg : this_node.edges)
					calc_weight(cut_part, lat[eg.second.nbkey], weights[eg.second.nbkey], part);

				temp_subparts = subparts;
				further_gain = refine(lat, part, temp_subparts);

				if (cumu_gain * further_gain > max_gain) {
					max_gain = cumu_gain * further_gain;
					best_subparts = temp_subparts;
				}
				++i;
			}
		}

		// for(auto & i: std::get<0>(lat["ten0_1"])->val ) std::cout<<i<<"
		// **************6\n"; 	std::cout<<"finish **************6\n";

		if (contract_test_mode)
			lat.draw("lat after adjustment", best_subparts, true);

		if (contract_trace_mode)
			std::cout << "out_adjust \n";

		subparts = best_subparts;
	}

	template <typename KeySetType>
	struct keyset {
		KeySetType node_set;
		static keyset<KeySetType> absorb(const keyset<KeySetType> & a, const int & b) {
			return a;
		};
		static keyset<KeySetType> contract(const keyset<KeySetType> & a, const keyset<KeySetType> & b) {
			keyset<KeySetType> r = a;
			r.node_set.insert(b.node_set.begin(), b.node_set.end());
			return r;
		}
		keyset() = default;
		template <typename NodeType>
		keyset(const typename KeySetType::key_type & k, const NodeType & n) {
			node_set.insert(k);
		}
		std::string show(){
			return "";
		}
	};

	template <typename KeySetType>
	struct contract_info {
		KeySetType node_set;
		long long int this_weight = 1;
		long long int hist_max_weight = 1;
		long long int contraction_cost = 1;
		long long int legs = 1;
		static contract_info<KeySetType> absorb(contract_info<KeySetType> & c, const int & d) {
			contract_info<KeySetType> r = c;
			r.legs *= d;
			return r;
		};
		static contract_info<KeySetType> contract(contract_info<KeySetType> & c, contract_info<KeySetType> & d) {
			contract_info<KeySetType> r;
			r.legs = 1;
			r.node_set = c.node_set;
			r.node_set.insert(d.node_set.begin(), d.node_set.end());
			r.this_weight = c.this_weight / c.legs / d.legs * d.this_weight / c.legs / d.legs;
			r.contraction_cost = c.contraction_cost + d.contraction_cost + c.this_weight / c.legs / d.legs * d.this_weight;
			r.hist_max_weight = std::max(std::max(c.this_weight, d.this_weight), r.this_weight);
			c.legs=1;
			d.legs=1;
			return r;
		}
		std::string show(){
			std::ostringstream os;
			os.precision(4);
			os<<std::fixed<<"C "<<contraction_cost<<"\nW "<<this_weight<<"\nH "<<hist_max_weight;
			return os.str();
		}
		contract_info() = default;
		template <typename NodeType>
		contract_info(const typename KeySetType::key_type & k, const NodeType & n) {
			node_set.insert(k);
			this_weight = net::tensor::get_size(n);
			hist_max_weight = this_weight;
		}
	};

	constexpr double exp_sum_log(const double & a,
										  const double & b) { // return log(exp(a)+exp(b))
		double ratio = 0.;
		if (a > b + 10) {
			ratio = std::pow(10,b - a);
			return a + ratio - ratio * ratio / 2. + ratio * ratio * ratio / 3.;
		} else if (b > a + 10) {
			ratio = std::pow(10,a - b);
			return b + ratio - ratio * ratio / 2. + ratio * ratio * ratio / 3.;
		} else
			return b + std::log10(1 + std::pow(10,a - b));
	}

	template <typename KeySetType>
	struct contract_info2 {
		KeySetType node_set;
		double this_weight = 0.;
		double hist_max_weight = 0.;
		double contraction_cost = 0.;
		double legs = 0.;
		static contract_info2<KeySetType> absorb(contract_info2<KeySetType> & c, const int & d) {
			contract_info2<KeySetType> r = c;
			r.legs += std::log10(double(d));
			return r;
		}
		static contract_info2<KeySetType> contract(contract_info2<KeySetType> & c, contract_info2<KeySetType> & d) {
			contract_info2<KeySetType> r;
			r.legs = 0.;
			r.node_set = c.node_set;
			// std::cout<<"before\n";
			// for (auto & test: d.node_set){
			// 	std::cout<<'\n';
			// 	std::cout<<test<<'\n';
			// } 
			r.node_set.insert(d.node_set.begin(), d.node_set.end());

			//std::cout<<"after\n";
			r.this_weight = c.this_weight + d.this_weight - 2 * c.legs - 2 * d.legs;
			r.contraction_cost = exp_sum_log(exp_sum_log(c.contraction_cost, d.contraction_cost), c.this_weight - c.legs - d.legs + d.this_weight);
			r.hist_max_weight = std::max(std::max(c.this_weight, d.this_weight), r.this_weight);
			c.legs=0.;
			d.legs=0.;
			return r;
		}
		std::string show(){
			std::ostringstream os;
			os.precision(4);
			os<<std::fixed<<"C "<<contraction_cost<<"\nW "<<this_weight<<"\nH "<<hist_max_weight;
			return os.str();
		}
		contract_info2() = default;
		template <typename NodeType>
		contract_info2(const typename KeySetType::key_type & k, const NodeType & n) {
			node_set.insert(k);
			this_weight = std::log10(double(net::tensor::get_size(n)));
			hist_max_weight = this_weight;
		}
	};

	template <template <typename> typename TreeVal, typename NetType>
	net::tree<TreeVal<typename NetType::NodeKeySetType>> * get_contract_tree(const NetType & lat, Engine & eg) {
		net::network<net::tree<TreeVal<typename NetType::NodeKeySetType>> *, int, typename NetType::NodeKeyType, typename NetType::EdgeKeyType> temp;

		temp = lat.template gfmap<decltype(temp)>(
				[](const typename NetType::NodeKeyType & nodek, const typename NetType::NodeValType & nodev) {
					return new net::tree<TreeVal<typename NetType::NodeKeySetType>>(TreeVal<typename NetType::NodeKeySetType>(nodek, nodev));
				},
				[](const typename NetType::NodeKeyType & nodek1,
					const typename NetType::NodeValType & nodev1,
					const typename NetType::NodeKeyType & nodek2,
					const typename NetType::NodeValType & nodev2,
					const typename NetType::EdgeKeyType & ind1,
					const typename NetType::EdgeKeyType & ind2,
					const typename NetType::EdgeValType & ev) { return net::tensor::get_dim(nodev1, ind1); });

		std::set<std::string> includes;
		for (auto & n : lat)
			includes.insert(n.first);
		return eg.contract(
				temp,
				includes,
				net::Tree_act<TreeVal<typename NetType::NodeKeySetType>>(),
				net::Tree_combine<TreeVal<typename NetType::NodeKeySetType>>());
	}

	template <template <typename> typename TreeVal, typename NetType>
	net::tree<TreeVal<typename NetType::NodeKeySetType>> * get_contract_tree_qbb(const NetType & lat, Engine & eg) {
		net::network<net::tree<TreeVal<typename NetType::NodeKeySetType>> *, int, typename NetType::NodeKeyType, typename NetType::EdgeKeyType> temp;

		temp = lat.template gfmap<decltype(temp)>(
				[](const typename NetType::NodeKeyType & nodek, const typename NetType::NodeValType & nodev) {
					return new net::tree<TreeVal<typename NetType::NodeKeySetType>>(TreeVal<typename NetType::NodeKeySetType>(nodek, nodev));
				},
				[](const typename NetType::NodeKeyType & nodek1,
					const typename NetType::NodeValType & nodev1,
					const typename NetType::NodeKeyType & nodek2,
					const typename NetType::NodeValType & nodev2,
					const typename NetType::EdgeKeyType & ind1,
					const typename NetType::EdgeKeyType & ind2,
					const typename NetType::EdgeValType & ev) { return net::tensor::get_dim(nodev1, ind1); });

		std::set<std::string> includes;
		for (auto & n : lat)
			includes.insert(n.first);
		return eg.contract_qbb(
				temp,
				includes,
				net::Tree_act<TreeVal<typename NetType::NodeKeySetType>>(),
				net::Tree_combine<TreeVal<typename NetType::NodeKeySetType>>());
	}

	template <template <typename> typename TreeVal, typename NetType>
	net::tree<TreeVal<typename NetType::NodeKeySetType>> * get_contract_tree_naive(const NetType & lat, Engine & eg) {
		net::network<net::tree<TreeVal<typename NetType::NodeKeySetType>> *, int, typename NetType::NodeKeyType, typename NetType::EdgeKeyType> temp;

		temp = lat.template gfmap<decltype(temp)>(
				[](const typename NetType::NodeKeyType & nodek, const typename NetType::NodeValType & nodev) {
					return new net::tree<TreeVal<typename NetType::NodeKeySetType>>(TreeVal<typename NetType::NodeKeySetType>(nodek, nodev));
				},
				[](const typename NetType::NodeKeyType & nodek1,
					const typename NetType::NodeValType & nodev1,
					const typename NetType::NodeKeyType & nodek2,
					const typename NetType::NodeValType & nodev2,
					const typename NetType::EdgeKeyType & ind1,
					const typename NetType::EdgeKeyType & ind2,
					const typename NetType::EdgeValType & ev) { return net::tensor::get_dim(nodev1, ind1); });

		std::set<std::string> includes;
		for (auto & n : lat)
			includes.insert(n.first);
		return eg.contract_naive(
				temp,
				includes,
				net::Tree_act<TreeVal<typename NetType::NodeKeySetType>>(),
				net::Tree_combine<TreeVal<typename NetType::NodeKeySetType>>());
	}

} // namespace net
#endif
