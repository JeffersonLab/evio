//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_STRUCTUREFINDER_H
#define EVIO_6_0_STRUCTUREFINDER_H


#include "vector"
#include "memory"


#include "IEvioFilter.h"
#include "IEvioListener.h"
#include "EvioXMLDictionary.h"
#include "BaseStructure.h"
#include "StructureType.h"


namespace evio {


    /**
     * This is a set of convenient static methods used to find lists of structures
     * within an event, bank, segment, or tagsegment that match certain criteria. For the most
     * part it uses the <code>{@link BaseStructure#getMatchingStructures()}</code>
     * method on the provided <code>EvioEvent</code> object by constructing the
     * appropriate filter.
     *
     * @author heddle
     * @author timmer
     */
    class StructureFinder {

      public:

        /**
         * Collect all the structures in an event that pass a filter.
         * @param structure the event/bank/seg/tagseg being queried.
         * @param filter the filter that must be passed. If <code>null</code>, this will return all the structures.
         * @param vec    vector provided to contain all structures that are accepted by the filter for the provided event.
         */
        static void getMatchingStructures(std::shared_ptr<BaseStructure> structure,
                                         std::shared_ptr<IEvioFilter> filter,
                                         std::vector<std::shared_ptr<BaseStructure>> & vec) {
            if (structure == nullptr) {
                std::cout << "getMatchingStructures: returning null list" << std::endl;
                vec.clear();
                return;
            }
            return structure->getMatchingStructures(filter, vec);
        }

        /**
         * Collect all the banks in an event that match a provided tag and number in their header.
         * Only Banks are returned, because only Banks have a number field.
         * @param structure the event/bank/seg/tagseg being queried.
         * @param tag  the tag to match.
         * @param num  the number to match.
         * @param vec  vector provided to contain all Banks that are accepted by the filter for the provided event.
         */
        static void getMatchingBanks(std::shared_ptr<BaseStructure> structure,
                                     uint16_t tag, uint8_t num,
                                     std::vector<std::shared_ptr<BaseStructure>> & vec) {

            class myFilter : public IEvioFilter {
                uint16_t tag; uint8_t num;
            public:
                myFilter(uint16_t tag, uint8_t num) : tag(tag), num(num) {}
                bool accept(StructureType const & type, std::shared_ptr<BaseStructure> struc) override {
                    return ((type == StructureType::STRUCT_BANK) &&
                            (tag == struc->getHeader()->getTag()) &&
                            (num == struc->getHeader()->getNumber()));
                }
             };

            auto filter = std::make_shared<myFilter>(tag, num);
            return getMatchingStructures(structure, filter, vec);
        }

        /**
         * Collect all the structures in an event that match a provided tag in their header.
         * @param structure the event/bank/seg/tagseg being queried.
         * @param tag the tag to match.
         * @param vec vector provided to contain all structures that are accepted by the filter for the provided event.
         */
        static void getMatchingStructures(std::shared_ptr<BaseStructure> structure, uint16_t tag,
                                          std::vector<std::shared_ptr<BaseStructure>> & vec) {

            class myFilter : public IEvioFilter {
                uint16_t tag;
            public:
                myFilter(uint16_t tag) : tag(tag) {}
                bool accept(StructureType const & structureType, std::shared_ptr<BaseStructure> struc) override {
                    return (tag == struc->getHeader()->getTag());
                }
            };

            auto filter = std::make_shared<myFilter>(tag);
            return getMatchingStructures(structure, filter, vec);
        }

        /**
         * Collect all the non-banks (i.e., Segments and TagSegments) in an event that match
         * a provided tag in their header. No Banks are returned.
         * @param structure the event/bank/seg/tagseg being queried.
         * @param tag the tag to match.
         * @param vec  vector provided to contain all non-bank structures that are accepted by the filter for the provided event.
         */
        static void getMatchingNonBanks(std::shared_ptr<BaseStructure> structure, uint16_t tag,
                                        std::vector<std::shared_ptr<BaseStructure>> & vec) {

            class myFilter : public IEvioFilter {
                uint16_t tag;
            public:
                myFilter(uint16_t tag) : tag(tag) {}
                bool accept(StructureType const & type, std::shared_ptr<BaseStructure> struc) override {
                    return ((type != StructureType::STRUCT_BANK) &&
                            (tag == struc->getHeader()->getTag()));
                }
            };

            auto filter = std::make_shared<myFilter>(tag);
            return getMatchingStructures(structure, filter, vec);
        }


        /**
         * Collect all structures in an event that match the given dictionary name.
         *
         * @param structure the event/bank/seg/tagseg being queried.
         * @param name       dictionary name of structures to be returned.
         * @param dictionary dictionary to be used.
         * @param vec        vector provided to contain BaseStructures that
         *                   have the given name in the given dictionary.
         */
        static void getMatchingStructures(std::shared_ptr<BaseStructure> structure,
                                          std::string name,
                                          EvioXMLDictionary & dictionary,
                                          std::vector<std::shared_ptr<BaseStructure>> & vec) {

            // This IEvioFilter selects structures that match the given dictionary name
            class myFilter : public IEvioFilter {
                std::string name;
                EvioXMLDictionary & dict;
            public:
                myFilter(std::string const & name, EvioXMLDictionary & dict) :
                        name(name), dict(dict) {}

                bool accept(StructureType const & structureType,
                            std::shared_ptr<BaseStructure> struc) override {
                    // If this structure matches the name, add it to the list
                    return (name == dict.getName(struc));
                }
            };

            auto filter = std::make_shared<myFilter>(name, dictionary);
            return getMatchingStructures(structure, filter, vec);
        }


        /**
         * Collect all structures in an event whose <b>parent</b> has the given dictionary name.
         *
         * @param structure  the event/bank/seg/tagseg being queried.
         * @param parentName dictionary name of parent of structures to be returned.
         * @param dictionary dictionary to be used.
         * @param vec        vector provided to contain BaseStructures whose
         *                   parent has the given name in the given dictionary.
         */
        static void getMatchingParent(std::shared_ptr<BaseStructure> structure,
                                     std::string parentName,
                                     EvioXMLDictionary & dictionary,
                                     std::vector<std::shared_ptr<BaseStructure>> & vec) {

            // This IEvioFilter selects structures whose parent has the given dictionary name
            class myFilter : public IEvioFilter {
                std::string name;
                EvioXMLDictionary & dict;
            public:
                myFilter(std::string const & name, EvioXMLDictionary & dict) :
                        name(name), dict(dict) {}

                bool accept(StructureType const & structureType,
                            std::shared_ptr<BaseStructure> struc) override {

                    auto parent = struc->getParent();
                    if (parent == nullptr) {
                        return false;
                    }

                    // If this parent matches the name, add it to the list
                    return (name == dict.getName(parent));
                }
            };

            auto filter = std::make_shared<myFilter>(parentName, dictionary);
            return getMatchingStructures(structure, filter, vec);
        }


        /**
         * Collect all structures in an event who has a <b>child</b> with the given dictionary name.
         *
         * @param structure  the event/bank/seg/tagseg being queried.
         * @param childName  dictionary name of a child of structures to be returned.
         * @param dictionary dictionary to be used; if null, an existing global dictionary will be used.
         * @param vec        vector provided to contain BaseStructures who
         *                   have a child with the given name in the given dictionary.
         */
        static void getMatchingChild(std::shared_ptr<BaseStructure> structure,
                                     std::string childName,
                                     EvioXMLDictionary & dictionary,
                                     std::vector<std::shared_ptr<BaseStructure>> & vec) {

            // This IEvioFilter selects structures who have a child with the given dictionary name
            class myFilter : public IEvioFilter {
                std::string name;
                EvioXMLDictionary & dict;
            public:
                myFilter(std::string const & name, EvioXMLDictionary & dict) :
                        name(name), dict(dict) {}

                bool accept(StructureType const & structureType,
                            std::shared_ptr<BaseStructure> struc) override {

                    auto children = struc->getChildren();
                    if (children.empty()) {
                        return false;
                    }

                    for (auto child : children) {
                        if (name == dict.getName(child)) {
                            // If this child matches the name, add it to the list
                            return true;
                        }
                    }

                    return false;
                }
            };

            auto filter = std::make_shared<myFilter>(childName, dictionary);
            return getMatchingStructures(structure, filter, vec);
        }

    };


}

#endif //EVIO_6_0_STRUCTUREFINDER_H
