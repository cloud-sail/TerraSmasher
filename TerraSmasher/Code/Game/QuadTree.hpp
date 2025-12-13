#pragma once
//#include "Engine/Math/Vec2.hpp"
//#include "Engine/Math/AABB2.hpp"
//
//#include <vector>
//#include <algorithm>
//#include <memory>
//#include <functional>
//
//struct QuadTreeObject {
//	float x, y;
//
//	QuadTreeObject(float x = 0, float y = 0) : x(x), y(y) {}
//	virtual ~QuadTreeObject() {}
//};
//
//template<typename T = QuadTreeObject>
//class QuadTree {
//private:
//	struct Node {
//		AABB2 boundary;  
//		std::vector<T*> objects;
//		Node* children[4];
//		Node* parent;
//		int depth;
//
//		Node(const AABB2& box, Node* parent = nullptr, int depth = 0)
//			: boundary(box), parent(parent), depth(depth) 
//		{
//			for (int i = 0; i < 4; i++) 
//			{
//				children[i] = nullptr;
//			}
//		}
//
//		~Node() {
//			Clear();
//		}
//
//		bool IsLeaf() const 
//		{
//			return children[0] == nullptr;
//		}
//
//		void Clear() 
//		{
//			for (int i = 0; i < 4; i++) {
//				if (children[i]) {
//					delete children[i];
//					children[i] = nullptr;
//				}
//			}
//			objects.clear();
//		}
//
//		int GetTotalObjectCount() const 
//		{
//			int count = objects.size();
//			if (!IsLeaf()) {
//				for (int i = 0; i < 4; i++) {
//					if (children[i]) {
//						count += children[i]->GetTotalObjectCount();
//					}
//				}
//			}
//			return count;
//		}
//	};
//
//	Node* root;
//
//	int maxObjects; 
//	int maxLevels;
//	int mergeThreshold;
//
//public:
//	QuadTree(const Box& boundary, int maxObjects = 4, int maxLevels = 5, int mergeThreshold = 2)
//		: maxObjects(maxObjects), maxLevels(maxLevels), mergeThreshold(mergeThreshold) {
//		root = new Node(boundary);
//	}
//
//	~QuadTree() {
//		delete root;
//	}
//
//	bool Insert(T* object) {
//		if (!object) return false;
//		return InsertIntoNode(root, object);
//	}
//
//	bool Remove(T* object) {
//		if (!object) return false;
//		return RemoveFromNode(root, object);
//	}
//
//	void Query(const Box& range, std::vector<T*>& results) {
//		QueryNode(root, range, results);
//	}
//
//	void QueryPoint(float x, float y, std::vector<T*>& results) {
//		QueryPointNode(root, x, y, results);
//	}
//
//	void Clear() {
//		root->Clear();
//	}
//
//	int GetObjectCount() const {
//		return root->GetTotalObjectCount();
//	}
//
//	void ForEach(std::function<void(T*)> callback) {
//		ForEachNode(root, callback);
//	}
//
//private:
//	int GetQuadrant(const Box& nodeBoundary, float objX, float objY) const {
//		float midX = nodeBoundary.x + nodeBoundary.width / 2.0f;
//		float midY = nodeBoundary.y + nodeBoundary.height / 2.0f;
//
//		bool top = (objY < midY);
//		bool left = (objX < midX);
//
//		// 0 = top left, 1 = top right, 2 = bottom left, 3 = bottom right
//		if (top && left) return 0;
//		if (top && !left) return 1;
//		if (!top && left) return 2;
//		if (!top && !left) return 3;
//
//		return -1;
//	}
//
//	void Split(Node* node) {
//		if (!node || !node->IsLeaf()) return;
//
//		float subWidth = node->boundary.width / 2.0f;
//		float subHeight = node->boundary.height / 2.0f;
//		float x = node->boundary.x;
//		float y = node->boundary.y;
//
//		node->children[0] = new Node(Box(x, y, subWidth, subHeight),
//			node, node->depth + 1);
//		node->children[1] = new Node(Box(x + subWidth, y, subWidth, subHeight),
//			node, node->depth + 1);
//		node->children[2] = new Node(Box(x, y + subHeight, subWidth, subHeight),
//			node, node->depth + 1);
//		node->children[3] = new Node(Box(x + subWidth, y + subHeight, subWidth, subHeight),
//			node, node->depth + 1);
//
//		std::vector<T*> objectsToRedistribute = node->objects;
//		node->objects.clear();
//
//		for (T* obj : objectsToRedistribute) {
//			int index = GetQuadrant(node->boundary, obj->x, obj->y);
//			if (index != -1 && node->children[index]) {
//				node->children[index]->objects.push_back(obj);
//			}
//			else {
//				// not in any quadrant
//				node->objects.push_back(obj);
//			}
//		}
//	}
//
//	bool InsertIntoNode(Node* node, T* object) {
//		if (!node) return false;
//
//		if (!node->boundary.Contains(object->x, object->y)) {
//			return false;
//		}
//
//		if (node->IsLeaf()) {
//
//			node->objects.push_back(object);
//			// need split?
//			if (node->objects.size() > maxObjects && node->depth < maxLevels) {
//				Split(node);
//			}
//			return true;
//		}
//
//		// not leaf
//		int index = GetQuadrant(node->boundary, object->x, object->y);
//		if (index != -1 && node->children[index]) {
//			return InsertIntoNode(node->children[index], object);
//		}
//
//		node->objects.push_back(object);
//		return true;
//	}
//
//	bool RemoveFromNode(Node* node, T* object) {
//		if (!node) return false;
//
//		auto it = std::find(node->objects.begin(), node->objects.end(), object);
//		if (it != node->objects.end()) {
//			node->objects.erase(it);
//			MergeNode(node);
//			return true;
//		}
//
//		// if has leaf, recursively search
//		if (!node->IsLeaf()) {
//			int index = GetQuadrant(node->boundary, object->x, object->y);
//			if (index != -1 && node->children[index]) {
//				if (RemoveFromNode(node->children[index], object)) {
//					// if child remove something try merge
//					MergeNode(node);
//					return true;
//				}
//			}
//			else {
//				// is this necessary?
//				for (int i = 0; i < 4; i++) {
//					if (node->children[i]) {
//						if (RemoveFromNode(node->children[i], object)) {
//							MergeNode(node);
//							return true;
//						}
//					}
//				}
//			}
//		}
//
//		return false;
//	}
//
//	void MergeNode(Node* node) {
//		if (!node || node->IsLeaf()) {
//			return;
//		}
//
//		bool allChildrenAreLeaves = true;
//		int totalObjects = 0;
//
//		for (int i = 0; i < 4; i++) {
//			if (node->children[i]) {
//				if (!node->children[i]->IsLeaf()) {
//					allChildrenAreLeaves = false;
//					break;
//				}
//				totalObjects += node->children[i]->objects.size();
//			}
//		}
//
//		totalObjects += node->objects.size();
//
//		if (allChildrenAreLeaves && totalObjects <= mergeThreshold) {
//			for (int i = 0; i < 4; i++) {
//				if (node->children[i]) {
//					node->objects.insert(
//						node->objects.end(),
//						node->children[i]->objects.begin(),
//						node->children[i]->objects.end()
//					);
//
//					delete node->children[i];
//					node->children[i] = nullptr;
//				}
//			}
//		}
//	}
//
//	void QueryNode(Node* node, const Box& range, std::vector<T*>& results) {
//		if (!node) return;
//
//		if (!node->boundary.Intersects(range)) {
//			return;
//		}
//
//		for (T* obj : node->objects) {
//			if (range.Contains(obj->x, obj->y)) {
//				results.push_back(obj);
//			}
//		}
//
//		if (!node->IsLeaf()) {
//			for (int i = 0; i < 4; i++) {
//				if (node->children[i]) {
//					QueryNode(node->children[i], range, results);
//				}
//			}
//		}
//	}
//
//	void QueryPointNode(Node* node, float x, float y, std::vector<T*>& results) {
//		if (!node) return;
//
//		if (!node->boundary.Contains(x, y)) {
//			return;
//		}
//
//		results.insert(results.end(), node->objects.begin(), node->objects.end());
//
//		if (!node->IsLeaf()) {
//			int index = GetQuadrant(node->boundary, x, y);
//			if (index != -1 && node->children[index]) {
//				QueryPointNode(node->children[index], x, y, results);
//			}
//		}
//	}
//
//	void ForEachNode(Node* node, std::function<void(T*)>& callback) {
//		if (!node) return;
//
//		for (T* obj : node->objects) {
//			callback(obj);
//		}
//
//		if (!node->IsLeaf()) {
//			for (int i = 0; i < 4; i++) {
//				if (node->children[i]) {
//					ForEachNode(node->children[i], callback);
//				}
//			}
//		}
//	}
//};
