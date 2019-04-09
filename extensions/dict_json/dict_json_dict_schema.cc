const char * dict_json_dict_schema = "\
{ \n\
    \"definitions\": { \n\
        \"content\": { \n\
            \"type\": \"array\", \n\
            \"items\": { \n\
                \"type\": \"object\", \n\
                \"additionalProperties\": false, \n\
                \"required\": [ \"AVP\" ], \n\
                \"properties\": { \n\
                    \"AVP\": { \"type\": \"string\" }, \n\
                    \"Vendor\": { \"$ref\": \"#/definitions/unsigned-integer\" }, \n\
                    \"First\": { \"type\": \"boolean\" }, \n\
                    \"Min\": { \"$ref\": \"#/definitions/unsigned-integer\" }, \n\
                    \"Max\": { \"anyOf\": [ { \"type\": \"integer\" }, { \"enum\": [ \"unbounded\" ] } ] } \n\
                } \n\
            } \n\
        }, \n\
 \n\
        \"identifier\": { \"type\": \"string\", \"pattern\": \"^[[:print:]]+$\" }, \n\
        \"type\": { \n\
            \"enum\": [ \n\
                \"Address\", \n\
                \"DiameterIdentity\", \n\
                \"DiameterURI\", \n\
                \"Enumerated\", \n\
                \"Float32\", \n\
                \"Float64\", \n\
                \"Grouped\", \n\
                \"Integer32\", \n\
                \"Integer64\", \n\
                \"IPFilterRule\", \n\
                \"OctetString\", \n\
                \"Time\", \n\
                \"Unsigned32\", \n\
                \"Unsigned64\", \n\
                \"UTF8String\" \n\
            ] \n\
        }, \n\
        \"unsigned-integer\": { \"type\": \"integer\", \"minimum\": 0 } \n\
    }, \n\
     \n\
    \"type\": \"object\", \n\
    \"additionalProperties\": false, \n\
    \"properties\": { \n\
        \"Vendors\": { \n\
            \"type\": \"array\", \n\
            \"items\": { \n\
                \"type\": \"object\", \n\
                \"additionalProperties\": false, \n\
                \"required\": [ \"Code\", \"Name\" ], \n\
                \"properties\": { \n\
                    \"Code\": { \"$ref\": \"#/definitions/unsigned-integer\" }, \n\
                    \"Name\": { \"$ref\": \"#/definitions/identifier\" } \n\
                } \n\
            } \n\
        }, \n\
        \"Types\": { \n\
            \"type\": \"array\", \n\
            \"items\": { \n\
                \"type\": \"object\", \n\
                \"additionalProperties\": false, \n\
                \"required\": [ \"Name\", \"Base\" ], \n\
                \"properties\": { \n\
                    \"Name\": { \"type\": \"string\" }, \n\
                    \"Base\": { \"type\": \"string\" } \n\
                } \n\
            } \n\
        }, \n\
        \"AVPs\": { \n\
            \"type\": \"array\", \n\
            \"items\": { \n\
                \"type\": \"object\", \n\
                \"additionalProperties\": false, \n\
                \"required\": [ \"Code\", \"Name\", \"Type\" ], \n\
                \"properties\": { \n\
                    \"Code\": { \"$ref\": \"#/definitions/unsigned-integer\" }, \n\
                    \"Vendor\": { \"$ref\": \"#/definitions/unsigned-integer\" }, \n\
                    \"Name\": { \"$ref\": \"#/definitions/identifier\" }, \n\
                    \"Flags\": { \n\
                        \"type\": \"object\", \n\
                        \"additionalProperties\": false, \n\
                        \"properties\": { \n\
                            \"Must\": { \"type\": \"string\", \"pattern\": \"^[VMP]*$\" }, \n\
                            \"MustNot\": { \"type\": \"string\", \"pattern\": \"^[VMP]*$\" } \n\
                        } \n\
                    }, \n\
                    \"Type\": { \"$ref\": \"#/definitions/identifier\" }, \n\
                    \"EnumValues\": { \n\
                        \"type\": \"array\", \n\
                        \"items\": { \n\
                            \"type\": \"object\", \n\
                            \"additionalProperties\": false, \n\
                            \"required\": [ \"Code\", \"Name\" ], \n\
                            \"properties\": { \n\
                                \"Code\": { \"anyOf\": [ { \"type\": \"integer\" }, { \"type\": \"number\" }, { \"type\": \"string\" } ] }, \n\
                                \"Name\": { \"type\": \"string\", \"pattern\": \"^[[:print:]]*$\" } \n\
                            } \n\
                        } \n\
                    } \n\
                } \n\
            } \n\
        }, \n\
        \"Applications\": { \n\
            \"type\": \"array\", \n\
            \"items\": { \n\
                \"type\": \"object\", \n\
                \"additionalProperties\": false, \n\
                \"required\": [ \"Code\", \"Name\" ], \n\
                \"properties\": { \n\
                    \"Code\": { \"$ref\": \"#/definitions/unsigned-integer\" }, \n\
                    \"Name\": { \"$ref\": \"#/definitions/identifier\" } \n\
                } \n\
            } \n\
        }, \n\
        \"Commands\": { \n\
            \"type\": \"array\", \n\
            \"items\": { \n\
                \"type\": \"object\", \n\
                \"additionalProperties\": false, \n\
                \"required\": [ \"Code\", \"Name\" ], \n\
                \"properties\": { \n\
                    \"Code\": { \"$ref\": \"#/definitions/unsigned-integer\" }, \n\
                    \"Name\": { \"$ref\": \"#/definitions/identifier\" }, \n\
                    \"Application\": { \"$ref\": \"#/definitions/identifier\" }, \n\
                    \"Flags\": { \n\
                        \"type\": \"object\", \n\
                        \"additionalProperties\": false, \n\
                        \"properties\": { \n\
                            \"Must\": { \"type\": \"string\", \"pattern\": \"^[RPE]*$\" }, \n\
                            \"MustNot\": { \"type\": \"string\", \"pattern\": \"^[RPET]*$\" } \n\
                        } \n\
                    } \n\
                } \n\
            } \n\
        }, \n\
        \"CommandRules\": { \n\
            \"type\": \"array\", \n\
            \"items\": { \n\
                \"type\": \"object\", \n\
                \"additionalProperties\": false, \n\
                \"required\": [ \"Command\", \"Content\" ], \n\
                \"properties\": { \n\
                    \"Command\": { \"type\": \"string\", \"minimum\": 0 }, \n\
                    \"Content\": { \"$ref\": \"#/definitions/content\" } \n\
                } \n\
            } \n\
        }, \n\
        \"AVPRules\": { \n\
            \"type\": \"array\", \n\
            \"items\": { \n\
                \"type\": \"object\", \n\
                \"additionalProperties\": false, \n\
                \"required\": [ \"AVP\", \"Content\" ], \n\
                \"properties\": { \n\
                    \"AVP\": { \"type\": \"string\" }, \n\
                    \"Vendor\": { \"type\": \"integer\", \"minimum\" : 0 }, \n\
                    \"Content\": { \"$ref\": \"#/definitions/content\" } \n\
                } \n\
            } \n\
        } \n\
    } \n\
} \n\
";
