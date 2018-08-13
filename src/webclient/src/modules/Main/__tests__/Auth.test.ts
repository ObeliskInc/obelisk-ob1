// -------------------------------------------------------------------------------------------------
// Copyright 2018 Obelisk Inc.
// -------------------------------------------------------------------------------------------------

// import { clearAllUsers, setUser } from '../actions'
// import { reducer } from '../reducer'
// import { User } from '../types'

// import { getUsers } from '../selectors'

// const user1: User = {
//   id: '123123',
//   orderId: 'O-9938DF847A7C',
//   name: 'Ken Carpenter',
//   street1: '1234 Main Street',
//   street2: 'Apt 8G',
//   city: 'New York City',
//   stateProv: 'New York',
//   zipPostal: '10123',
//   country: 'United States',
//   email: 'ken@email.com',
//   backupEmail: 'ken2@email.com',
//   phone: '234-567-8901',
//   refundAddr: 'xyz',
// }

// const user2: User = {
//   id: '234234',
//   orderId: 'O-867BC655DF87',
//   name: 'Jenny Johnson',
//   street1: '5678 West Street',
//   street2: 'Apt 69F',
//   city: 'San Francisco',
//   stateProv: 'California',
//   zipPostal: '90210',
//   country: 'United States',
//   email: 'bob@email.com',
//   phone: '345-678-9101',
//   refundAddr: 'abc',
// }

// describe('reducer', () => {
//   test('should set new user correctly', () => {
//     const initialState = { users: {} }
//     const action = setUser(user1)
//     const newState = reducer(initialState, action)
//     expect(newState).toEqual({ users: { [user1.id]: user1 } })
//   })

//   test('should replace existing user correctly', () => {
//     const initialState = { users: { [user1.id]: { ...user1, name: 'Luke Skywalker' } } }
//     const action = setUser(user1)
//     const newState = reducer(initialState, action)
//     expect(newState).toEqual({ users: { [user1.id]: user1 } })
//   })

//   test('should clear all users correctly', () => {
//     const initialState = { users: { [user1.id]: user1, [user2.id]: user2 } }
//     const action = clearAllUsers()
//     const newState = reducer(initialState, action)
//     expect(newState).toEqual({ users: {} })
//   })
// })

// describe('selectors', () => {
//   test('should get user map', () => {
//     const initialState = { users: { [user1.id]: user1, [user2.id]: user2 } }
//     const users = getUsers(initialState)
//     expect(users).toEqual({ [user1.id]: user1, [user2.id]: user2 })
//   })
// })
