import * as React from "react"
import { scrolled } from "react-stay-scrolled"

interface IProps {
  stayScrolled(): void
  text: any
}

class Message extends React.Component<IProps, {}> {
  componentDidMount() {
    const { stayScrolled } = this.props

    // Make the parent StayScrolled component scroll down if it was already scrolled
    stayScrolled()

    // Make the parent StayScrolled component scroll down, even if not completely scrolled down
    // scrollBottom();
  }

  render() {
    return <div>{this.props.text}</div>
  }
}

export default scrolled(Message)
