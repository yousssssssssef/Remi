#pragma once
// GraphQL operations the device sends to the Remi API (validated against backend/schema.graphql by tools/check_queries.mjs).
static const char Q_LOGIN[] =
    "mutation Login($e:String!,$p:String!){login(input:{email:$e,password:$p}){accessToken refreshToken expiresInSeconds}}";
static const char Q_REFRESH[] =
    "mutation Refresh($t:String!){refreshAccessToken(input:{refreshToken:$t}){accessToken refreshToken expiresInSeconds}}";
static const char Q_WHOAMI[] = "query Who{me{name} myHouseholds{household{id}}}";
static const char Q_INVENTORY[] =
    "query Kitchen($after:String){inventory(first:50,after:$after){nodes{foodId quantity estimatedExpiryAt} hasNextPage endCursor}}";
static const char Q_SHOPPING[] = "query Shopping{shoppingLists{status items{id foodId foodName quantity unit status}}}";
static const char Q_IDEAS[] =
    "query Ideas{recipeSuggestions(first:6){nodes{haveCount requiredCount recipe{id name cookTimeMin prepTimeMin}}}}";
static const char Q_RECIPE[] =
    "query Recipe($id:ID!){recipe(id:$id){id name cookTimeMin prepTimeMin steps{stepNumber instruction durationMin}}}";
static const char Q_ADD_STOCK[] = "mutation AddStock($i:InventoryStockInput!){addStock(input:$i){__typename}}";
static const char Q_MARK_BOUGHT[] =
    "mutation Bought($i:UpdateShoppingItemInputType!){updateShoppingListItem(input:$i){id status}}";
